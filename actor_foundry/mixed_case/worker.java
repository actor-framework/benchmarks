package osl.examples.caf_benches;
 
import osl.manager.*;
import osl.util.*;
import osl.manager.annotations.message;

import java.io.Serializable;

import java.util.List;
import java.util.ArrayList;

public class worker extends Actor {
  private static final long serialVersionUID = 4578547896541465872L;
//  public  static final      String class    =
//                              "osl.examples.caf_benches.worker";

  private ActorName m_mc = null;

  public worker(ActorName msgcollector) {
    super();
    m_mc = msgcollector;
  }

  @message
  public void calc(Long what) {
    // all major implementations of list implement Serilizable
    // (as does the ArrayList we use)
    send(m_mc, "result", (Serializable)factorize(what));
  }

  @message
  public void done() {
    destroy("done");
  }

  private List<Long> factorize(Long n) {
    List<Long> result = new ArrayList<Long>();
    if (n <= 3) {
      result.add(n);
      return result;
    }
    
    long d = 2;
    while (d < n) {
      if ((n % d) == 0) {
        result.add(d);
        n /= d;
      } else {
        d = (d == 2) ? 3 : (d + 2);
      }
    }
    result.add(d);
    return result;
  }
}
