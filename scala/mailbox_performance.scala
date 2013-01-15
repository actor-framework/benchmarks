package org.libcppa

import org.libcppa.utility._

import akka.actor._
import scala.annotation.tailrec

case object Msg

class Receiver(n: Long) extends Actor {
    var received: Long = 0
    def receive = {
        case Msg =>
            received += 1
            if (received == n) {
                global_latch.countDown
                context.stop(self)
            }
    }
}

object mailbox_performance {
    def usage() {
        Console println "usage: (num_threads) (msgs_per_thread)"
    }
    def main(args: Array[String]): Unit = args match {
        case Array(IntStr(threads), IntStr(msgs)) => {
            val system = ActorSystem()
            val testee = system.actorOf(Props(new Receiver(threads*msgs)))
            for (_ <- 0 until threads) {
                (new Thread {
                    override def run() { for (_ <- 0 until msgs) testee ! Msg }
                }).start
            }
            global_latch.await
            system.shutdown
            System.exit(0)
        }
        case _ => usage
    }
}
