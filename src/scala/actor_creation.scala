package org.caf.scala

import org.caf.scala.utility._

import akka.actor._
import scala.concurrent.duration.Duration
import scala.concurrent.Await

case class Spread(value: Int)
case class Result(value: Int)

class Testee(parent: ActorRef) extends Actor {
  def receive = {
    case Spread(1) =>
      parent ! Result(1)
      context.stop(self)
    case Spread(s) =>
      val msg = Spread(s - 1)
      context.actorOf(Props(new Testee(self))) ! msg
      context.actorOf(Props(new Testee(self))) ! msg
      context.become {
        case Result(r1) =>
          context.become {
            case Result(r2) =>
              if (parent == null) {
                val res = 2 + r1 + r2;
                val expected = (1 << s)
                if (res != expected) {
                  Console.println("Expected " + expected + ", found " + res)
                  System.exit(42)
                }
                context.stop(self)
                global_latch.countDown
              } else {
                parent ! Result(1 + r1 + r2)
                context.stop(self)
              }
            }
          }
  }
}

object actor_creation {
  def usage() {
    Console println "usage: POW\n       creates 2^POW actors"
  }
  def main(args: Array[String]): Unit = args match {
    case Array(IntStr(n)) => {
      val system = ActorSystem()
      system.actorOf(Props(new Testee(null))) ! Spread(n)
      global_latch.await
      Await.result(system.terminate, Duration.Inf)
      System.exit(0)
    }
    case _ => usage
  }
}
