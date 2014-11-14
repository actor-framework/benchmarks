package org.caf.scala

object utility {

    object IntStr {
        val IntRegex = "([0-9]+)".r
        def unapply(s: String): Option[Int] = s match {
            case IntRegex(`s`) => Some(s.toInt)
            case _ => None
        }
    }

    object KeyValuePair {
        val Rx = "([^=])+=([^=]*)".r
        def unapply(s: String): Option[Pair[String, String]] = s match {
            case Rx(key, value) => Some(Pair(key, value))
            case _ => None
        }
    }

    val global_latch = new java.util.concurrent.CountDownLatch(1)

}
