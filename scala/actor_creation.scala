package org.libcppa

import org.libcppa.utility._

import akka.actor._

case object GoAhead

case class Spread(value: Int)
case class Result(value: Int)

class Testee(parent: ActorRef) extends Actor {
    def receive = {
        case Spread(0) =>
            parent ! Result(1)
            context.stop(self)
        case Spread(s) =>
            val msg = Spread(s-1)
            context.actorOf(Props(new Testee(self))) ! msg
            context.actorOf(Props(new Testee(self))) ! msg
            context.become {
                case Result(r1) =>
                    context.become {
                        case Result(r2) =>
                            parent ! Result(r1 + r2)
                            context.stop(self)
                    }
            }
    }
}

class RootTestee(n: Int) extends Actor {
    def receive = {
        case GoAhead =>
            context.actorOf(Props(new Testee(self))) ! Spread(n)
        case Result(v) =>
            if (v != (1 << n)) {
                Console.println("Expected " + (1 << n) + ", received " + v)
                System.exit(42)
            }
            global_latch.countDown
            context.stop(self)
    }
}

object actor_creation {
    def usage() {
        Console println "usage: POW\n       creates 2^POW actors"
    }
    def main(args: Array[String]): Unit = args match {
        case Array(IntStr(n)) => {
            val system = ActorSystem()
            system.actorOf(Props(new RootTestee(n))) ! GoAhead
            global_latch.await
            system.shutdown
            System.exit(0)
        }
        case _ => usage
    }
}
