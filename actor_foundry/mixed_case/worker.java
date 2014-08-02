package osl.examples.caf_benches;
 
import osl.manager.*;
import osl.util.*;
import osl.manager.annotations.message;

import java.util.Vector;

public class worker extends Actor {
  private static final long serialVersionUID = 4578547896541465872L;
  public  static final      String _CLASS    =
                              "osl.examples.caf_benches.worker";

  private ActorName m_mc = null;
  
  @message
  public void init(ActorName msgcollector) {
    m_mc = msgcollector;
  }

  @message
  public void calc(Long what) {
    send(m_mc, "result", factorize(what));
  }

  @message
  public void done() {
    destroy("done");
  }

  private Vector<Long> factorize(Long n) {
    Vector<Long> result = new Vector<Long>();
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
