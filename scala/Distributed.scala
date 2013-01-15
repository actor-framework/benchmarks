package org.libcppa.distributed

import org.libcppa.utility.KeyValuePair
import org.libcppa.utility.IntStr

import akka.actor._
import com.typesafe.config.ConfigFactory

import scala.annotation.tailrec
import scala.concurrent.duration._

import Console.println

case class Ping(value: Int)
case class Pong(value: Int)
case class KickOff(value: Int)

case object Done
case class Ok(token: String)
case class Hello(token: String)
case class Olleh(token: String)
case class Error(msg: String, token: String)
case class AddPong(path: String, token: String)
case class SetParent(parent: ActorRef)


case class AddPongTimeout(path: String, token: String)

object global {
    val latch = new java.util.concurrent.CountDownLatch(1)
}

trait ServerActorPrototype[T] {

    protected def reply(what: Any): Unit
    protected def kickOff(old: T, value: Int): Unit
    protected def connectionEstablished(peers: T, pending: Any): T
    protected def newPending(peers: T, path: String, token: String) : T
    protected def handleTimeout(peers: T): (Boolean, T) = throw new RuntimeException("unsupported timeout")
    protected def handleAddPongTimeout(peers: T, path: String, token: String): (Boolean, T) = throw new RuntimeException("unsupported timeout")

    def recvFun(peers: T { def connected: List[{ def path: String }]; def pending: List[{ def clientToken: String }] }): PartialFunction[Any, (Boolean, T)] = {
        case Ping(value) => reply(Pong(value)); (false, peers)
        case Hello(token) => reply(Olleh(token)); (false, peers)
        case Olleh(token) => peers.pending find (_.clientToken == token) match {
            case Some(x) => (true, connectionEstablished(peers, x))
            case None => (false, peers)
        }
        case AddPong(path, token) => {
            //println("received AddPong(" + path + ", " + token + ")")
            if (peers.connected exists (_.path == path)) {
                reply(Ok(token))
                //println("recv[" + peers + "]: " + path + " cached (replied 'Ok')")
                (false, peers)
            }
            else {
                try { (true, newPending(peers, path, token)) }
                catch {
                    // catches match error and integer conversion failure
                    case e : Exception => reply(Error(e.toString, token)); (false, peers)
                }
            }
        }
        case KickOff(value) => kickOff(peers, value); (false, peers)
        case AddPongTimeout(path, token) => handleAddPongTimeout(peers, path, token)
        //case TIMEOUT => handleTimeout(peers)
    }
}

class PingActor(pongs: List[ActorRef]) extends Actor {

    import context.become

    private var parent: ActorRef = null
    private var left = pongs.length

    private def recvLoop: Receive = {
        case Pong(0) => {
            parent ! Done
            //println(parent.toString + " ! Done")
            if (left > 1) left -= 1
            else context.stop(self)
        }
        case Pong(value) => sender ! Ping(value - 1)
    }

    def receive = {
        case SetParent(p) => parent = p
        case KickOff(value) => pongs.foreach(_ ! Ping(value)); become(recvLoop)
    }
}

case class Peer(path: String, channel: ActorRef)
case class PendingPeer(path: String, channel: ActorRef, client: ActorRef, clientToken: String)
case class Peers(connected: List[Peer], pending: List[PendingPeer])

class ServerActor(system: ActorSystem) extends Actor with ServerActorPrototype[Peers] {

    import context.become

    protected  def reply(what: Any): Unit = sender ! what

    protected def kickOff(peers: Peers, value: Int): Unit = {
        val ping = context.actorOf(Props(new PingActor(peers.connected map (_.channel))))
        ping ! SetParent(sender)
        ping ! KickOff(value)
        //println("[" + peers + "]: KickOff(" + value + ")")
    }

    protected def connectionEstablished(peers: Peers, x: Any): Peers = x match {
        case PendingPeer(path, channel, client, token) => {
            client ! Ok(token)
            //println("connected to " + path)
            Peers(Peer(path, channel) :: peers.connected, peers.pending filterNot (_.clientToken == token))
        }
    }

    protected def newPending(peers: Peers, path: String, token: String) : Peers = {
        import context.dispatcher // Use this Actors' Dispatcher as ExecutionContext
        val channel = system.actorFor(path)
        channel ! Hello(token)
        system.scheduler.scheduleOnce(5 seconds, self, AddPongTimeout(path, token))
        //println("[" + peers + "]: sent 'Hello' to " + path)
        Peers(peers.connected, PendingPeer(path, channel, sender, token) :: peers.pending)
    }

    protected override def handleAddPongTimeout(peers: Peers, path: String, token: String) = {
        peers.pending find (x => x.path == path && x.clientToken == token) match {
            case Some(PendingPeer(_, channel, client, _)) => {
                client ! Error(path + " did not respond", token)
                //println(path + " did not respond")
                (true, Peers(peers.connected, peers.pending filterNot (x => x.path == path && x.clientToken == token)))
            }
            case None => (false, peers)
        }
    }

    def bhvr(peers: Peers): Receive = {
        case x => {
            recvFun(peers)(x) match {
                case (true, newPeers) => become(bhvr(newPeers))
                case _ => Unit
            }
        }
    }

    def receive = bhvr(Peers(Nil, Nil))

}

case class TokenTimeout(token: String)
case class RunClient(paths: List[String], numPings: Int)

class ClientActor(system: ActorSystem) extends Actor {

    import context.become

    def collectDoneMessages(left: Int): Receive = {
        case Done => {
//println("Done")
            if (left == 1) {
                global.latch.countDown
                context.stop(self)
            } else {
                become(collectDoneMessages(left - 1))
            }
        }
        case _ => {
            // ignore any other message
        }
    }

    def collectOkMessages(pongs: List[ActorRef], left: Int, receivedTokens: List[String], numPings: Int): Receive = {
        case Ok(token) => {
//println("Ok")
            if (left == 1) {
                //println("collected all Ok messages (wait for Done messages)")
                pongs foreach (_ ! KickOff(numPings))
                become(collectDoneMessages(pongs.length * (pongs.length - 1)))
            }
            else {
                become(collectOkMessages(pongs, left - 1, token :: receivedTokens, numPings))
            }
        }
        case TokenTimeout(token) => {
            if (!receivedTokens.contains(token)) {
                println("Error: " + token + " did not reply within 10 seconds")
                global.latch.countDown
                context.stop(self)
            }
        }
        case Error(what, token) => {
            println("Error [from " + token+ "]: " + what)
            global.latch.countDown
            context.stop(self)
        }
    }

    def receive = {
        case RunClient(paths, numPings) => {
//println("RunClient(" + paths.toString + ", " + numPings + ")")
            val pongs = paths map (x => {
                val pong = system.actorFor(x)
                paths foreach (y => if (x != y) {
                    val token = x + " -> " + y
                    pong ! AddPong(y, token)
//println(x + " ! AddPong(" + y + ", " + token + ")")
                    import context.dispatcher // Use this Actors' Dispatcher as ExecutionContext
                    system.scheduler.scheduleOnce(10 seconds, self, TokenTimeout(token))
                })
                pong
            })
            become(collectOkMessages(pongs, pongs.length * (pongs.length - 1), Nil, numPings))
        }
    }
}

class Distributed {

    def runServer() {
        val system = ActorSystem("pongServer", ConfigFactory.load.getConfig("pongServer"))
        system.actorOf(Props(new ServerActor(system)), "pong")
    }

    //private val NumPings = "num_pings=([0-9]+)".r
    private val SimpleUri = "([0-9a-zA-Z\\.]+):([0-9]+)".r

    @tailrec private def run(args: List[String], paths: List[String], numPings: Option[Int], finalizer: (List[String], Int) => Unit): Unit = args match {
        case KeyValuePair("num_pings", IntStr(num)) :: tail => numPings match {
            case Some(x) => throw new IllegalArgumentException("\"num_pings\" already defined, first value = " + x + ", second value = " + num)
            case None => run(tail, paths, Some(num), finalizer)
        }
        case arg :: tail => run(tail, arg :: paths, numPings, finalizer)
        case Nil => numPings match {
            case Some(x) => {
                if (paths.length < 2) throw new RuntimeException("at least two hosts required")
                finalizer(paths, x)
            }
            case None => throw new RuntimeException("no \"num_pings\" found")
        }
    }

    def runBenchmark(args: List[String]) {
        run(args, Nil, None, ((paths, x) => {
            val system = ActorSystem("benchmark", ConfigFactory.load.getConfig("benchmark"))
            system.actorOf(Props(new ClientActor(system))) ! RunClient(paths, x)
            global.latch.await
            system.shutdown
            System.exit(0)
        }))
    }

}

object Main {

    val prog = new Distributed

    def main(args: Array[String]): Unit = args match {
        case Array("mode=server") => prog.runServer
        // client mode
        case Array("mode=benchmark", _*) => prog.runBenchmark(args.toList.drop(2))
        // error
        case _ => {
            println("Running in server mode:\n"                              +
                    "  mode=server\n"                                        +
                    "  remote_actors PORT *or* akka\n"                       +
                    "\n"                                                     +
                    "Running the benchmark:\n"                               +
                    "  mode=benchmark\n"                                     +
                    "  remote_actors ... *or* akka\n"                         );
        }
    }

}
