package org.libcppa

import org.caf.scala.utility._

import akka.actor._
import com.typesafe.config.ConfigFactory

import scala.annotation.tailrec
import scala.concurrent.duration._
import scala.concurrent.Await

import Console.println

case class Ping(value: Int)
case class Pong(value: Int)
case class PingKickOff(value: Int, pong: ActorRef)
case class KickOff(value: Int)

case object Done
case class Ok(token: String)
case class AddPong(peerRef: ActorRef, token: String)


class PingActor(parent: ActorRef) extends Actor {

    import context.become

    private final def recvLoop: Receive = {
        case Pong(0) => {
            parent ! Done
            context.stop(self)
        }
        case Pong(value) => sender ! Ping(value - 1)
    }

    def receive = {
        case PingKickOff(value, pongServer) => {
          pongServer ! Ping(value); become(recvLoop)
        }
    }
}

class ServerActor(system: ActorSystem) extends Actor {

    import context.become

    private var peers = Set.empty[ActorRef]

    def receive = {
        case Ping(value) => sender ! Pong(value)
        case AddPong(peerRef, token) => {
            if (!peers.contains(peerRef)) {
                context.watch(peerRef)
                peers += peerRef
            }
            sender ! Ok(token)
        }
        case KickOff(value) => {
          // For each peer, spawn a PingActor to ping that peer.
          peers foreach { p =>
              val ping = context.actorOf(Props(classOf[PingActor], sender))
              ping ! PingKickOff(value, p)
          }
        }
    }
}

case class TokenTimeout(token: String)
case class RunClient(paths: List[String], numPings: Int)

class ClientActor(system: ActorSystem) extends Actor {

    import context.become

    def collectDoneMessages(left: Int): Receive = {
        case Done => {
//println("Done")
            if (left == 1) {
                context.system.log.info("Benchmark complete")
                global_latch.countDown
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
                context.system.log.error("Error: " + token + " did not reply within 10 seconds")
                global_latch.countDown
                context.stop(self)
            }
        }
    }

    def receive = {
        // Start the benchmark by sending n-1 AddPong messages to each pong server,
        // where n is the number of servers. Each server's path is listed in `paths`.
        case RunClient(paths, numPings) => {
//println("RunClient(" + paths.toString + ", " + numPings + ")")

            // Resolve ActorRefs to all pong servers.
            val t = 5.seconds
            val pongs = paths map (p =>
                Await.result(context.actorSelection(p).resolveOne(t), t)
            )

            // Start sending off AddPong.
            for { p1 <- pongs; p2 <- pongs; if p1 != p2 } {
                val token = p1.path.toStringWithAddress(p1.path.address) + " -> " + p2.path.toStringWithAddress(p2.path.address)
                p1 ! AddPong(p2, token)
                import context.dispatcher // Use this Actors' Dispatcher as ExecutionContext
                system.scheduler.scheduleOnce(10.seconds, self, TokenTimeout(token))
            }

            become(collectOkMessages(pongs, pongs.length * (pongs.length - 1), Nil, numPings))
        }
    }
}

object distributed {

    def runServer() {
        val system = ActorSystem("pongServer", ConfigFactory.load.getConfig("pongServer"))
        system.actorOf(Props(new ServerActor(system)), "pong")
    }

    //private val NumPings = "num_pings=([0-9]+)".r
    private val SimpleUri = "([0-9a-zA-Z\\.]+):([0-9]+)".r

    @tailrec private final def run(args: List[String], paths: List[String], numPings: Option[Int], finalizer: (List[String], Int) => Unit): Unit = args match {
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
            global_latch.await
            Await.result(system.terminate, Duration.Inf)
            System.exit(0)
        }))
    }

    def main(args: Array[String]): Unit = args match {
        case Array("mode=server") => runServer
        // client mode
        case Array("mode=benchmark", _*) => {
          runBenchmark(args.toList.drop(1))
        }
        // error
        case _ => {
            println("Running in server mode:\n"                              +
                    "  mode=server\n"                                        +
                    "\n"                                                     +
                    "Running the benchmark:\n"                               +
                    "  mode=benchmark\n"                                     +
                    "  num_pings=NUM\n"                                      +
                    "  PONG_ACTOR1 PONG_ACTOR2 [...]\n"                      +
                    "\n"                                                     +
                    "  where PONG_ACTOR is an actor path anchor, e.g.,\n"    +
                    "  akka.tcp://pongServer@hostname:2552/user/pong")
        }
    }

}
