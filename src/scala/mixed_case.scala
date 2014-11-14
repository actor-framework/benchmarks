package org.caf.scala

import org.caf.scala.utility._

import akka.actor._
import scala.annotation.tailrec

case class Token(value: Int)

case class Calc(value: Long)
case class Factors(values: List[Long])

case object Done
case object Init
case object MasterExited

object Global {
    val factor1: Long = 86028157
    val factor2: Long = 329545133
    val factors = List(factor2, factor1)
    val taskN: Long = factor1 * factor2
    def checkFactors(f: List[Long]) {
        assert(f equals factors)
    }
    @tailrec final def fac(n: Long, m: Long, interim: List[Long]) : List[Long] = {
        if (n == m) m :: interim
        else if ((n % m) == 0) fac(n/m, m, m :: interim)
        else fac(n, if (m == 2) 3 else m + 2, interim)
    }
    def factorize(arg: Long): List[Long] = {
        if (arg <= 3) List(arg)
        else fac(arg, 2, List())
    }
}

class Worker(supervisor: ActorRef) extends Actor {
    def receive = {
        case Calc(value) => supervisor ! Factors(Global.factorize(value))
        case Done => context.stop(self)
    }
}

class ChainLink(next: ActorRef) extends Actor {
    def receive = {
        case Token(value) => {
            next ! Token(value)
            if (value == 0) context.stop(self)
        }
    }
}

class ChainMaster(supervisor: ActorRef, worker: ActorRef, ringSize: Int, initialTokenValue: Int, repetitions: Int) extends Actor {

    @tailrec final def newRing(next: ActorRef, rsize: Int): ActorRef = {
        if (rsize == 0) next
        else newRing(context.actorOf(Props(new ChainLink(next))), rsize-1)
    }

    def running(next: ActorRef, iteration: Int): Receive = {
        case Token(0) =>
            if (iteration + 1 < repetitions) {
                worker ! Calc(Global.taskN)
                val new_next = newRing(self, ringSize - 1)
                new_next ! Token(initialTokenValue)
                context.become(running(new_next, iteration + 1))
            }
            else
            {
                worker ! Done
                supervisor ! MasterExited
                context.stop(self)
            }
        case Token(value) => next ! Token(value-1)
    }

    def receive = {
        case Init =>
            worker ! Calc(Global.taskN)
            val next = newRing(self, ringSize-1)
            next ! Token(initialTokenValue)
            context.become(running(next, 0))
    }
}

class Supervisor(numMessages: Int, numRings: Int, ringSize: Int, initialTokenValue: Int, repetitions: Int) extends Actor {
    def stopOnLast(left: Int) = {
        if (left == 1) {
            global_latch.countDown
            context.stop(self)
        }
    }
    def awaitMessages(left: Int): Receive = {
        case Factors(f) => Global.checkFactors(f); stopOnLast(left); context.become(awaitMessages(left-1))
        case MasterExited => stopOnLast(left); context.become(awaitMessages(left-1))
    }
    def receive = {
        case Init =>
            //println("expect " + numMessages + " messages")
            for (_ <- 0 until numRings) {
                val worker = context.actorOf(Props(new Worker(self)))
                context.actorOf(Props(new ChainMaster(self, worker, numRings, initialTokenValue, repetitions))) ! Init
            }
            context.become(awaitMessages(numMessages))
    }
}

object mixed_case {

    def usage() = {
        Console println "usage: (num rings) (ring size) (initial token value) (repetitions)"
        System.exit(1) // why doesn't exit return Nothing?
    }

    def main(args: Array[String]): Unit = args match {
        case Array(IntStr(numRings), IntStr(ringSize), IntStr(initToken), IntStr(reps)) => {
            val numMessages = numRings + (numRings * reps)
            val system = ActorSystem();
            val s = system.actorOf(Props(new Supervisor(numMessages, numRings, ringSize, initToken, reps)))
            s ! Init
            global_latch.await
            system.shutdown
            System.exit(0)
        }
        case _ => usage
    }

}
