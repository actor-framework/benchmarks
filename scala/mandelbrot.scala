/* The Computer Language Benchmarks Game
   http://benchmarksgame.alioth.debian.org/
   original contributed by Isaac Gouy
   made to use single array and parallelized by Stephen Marsh
   converted to Scala 2.8 by Rex Kerr
   added Akka
*/

package org.caf.scala

import akka.actor._
import org.caf.scala.utility._

import Array._
import java.io.BufferedOutputStream

object mandelbrot {

  var global_latch: java.util.concurrent.CountDownLatch = null

  var size: Int = 0
  var bytesPerRow: Int = 0
  var bitmap: Array[Byte] = _
  var donerows: Array[Boolean] = _
  var nextRow = 0
  val limitSquared = 4.0
  val max = 50

  case class Row(row: Int)

  def usage() = {
    print("usage: (number of pixels > 0)\n")
    System.exit(1) // why doesn't exit return Nothing?
  }

  class Worker extends Actor {
    def receive = {
      case Row(row) => {
        var bits = 0
        var bitnum = 0
        var x = 0
        var aindex = row * bytesPerRow
        while (x < size) {
          val cr = 2.0 * x / size - 1.5
          val ci = 2.0 * row / size - 1.0
          var zr, tr, zi, ti = 0.0
          var j = max
          do {
            zi = 2.0 * zr * zi + ci
            zr = tr - ti + cr
            ti = zi*zi
            tr = zr*zr
            j = j - 1
          } while (!(tr + ti > limitSquared) && j > 0)
          bits = bits << 1
          if (!(tr + ti > limitSquared)) {
            bits += 1
          }
          bitnum += 1
          if (x == size - 1) {
            bits = bits << (8 - bitnum)
            bitnum = 8
          }
          if (bitnum == 8) {
            bitmap(aindex) = bits.toByte
            aindex += 1
            bits = 0
            bitnum = 0
          }
          x += 1
        }
        context stop self
        global_latch countDown
      }
    }
  }

  def main(args: Array[String]) : Unit = args match {
    //case Array(IntStr(numPixels)) if numPixels == 0 => usage
    case Array(IntStr(numPixels)) => {
      size = numPixels
      bytesPerRow = (size+7)/8 // ceiling of (size / 8)
      bitmap = new Array(bytesPerRow*size)
      donerows = new Array(size)
      global_latch = new java.util.concurrent.CountDownLatch(numPixels)
      val system = ActorSystem()
      for (i <- 0 until size) {
        system.actorOf(Props[Worker]) ! Row(i)
      }
      global_latch.await
      // main thread prints rows as they finish
      //println("P4\n" + size + " " + size)
      //val w = new BufferedOutputStream(System.out)
      //var y = 0
      //while (y < size) {
      // w.write(bitmap, y * bytesPerRow, bytesPerRow)
      //  y += 1
      //}
      //w.close
      system.shutdown
      System.exit(0)
    }
    case _ => usage
  }
}
