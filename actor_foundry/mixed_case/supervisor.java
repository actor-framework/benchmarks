package osl.examples.caf_benches;
 
import osl.manager.*;
import osl.util.*;
import osl.manager.annotations.message;

import java.util.Vector;

public class supervisor extends Actor {
  private static final boolean DEBUG         = false;
  private static final long serialVersionUID = 4521476353751326142L;
  public  static final      String _CLASS    =
                              "osl.examples.caf_benches.supervisor";
  private static ActorName instance = null;

  private int m_left = 0;

  @message
  public void init(Integer left) {
    if (DEBUG) send(stdout, "println", "supervisor::init");
    instance = self();
    m_left = left.intValue();
  }
  
  @message
  public void masterdone() {
    if (DEBUG) send(stdout, "println", "supervisor::masterdone");
    if (--m_left == 0) {
      destroy("done");
    }
  }

  @message
  public void result(Vector<Long> vec) {
    if (DEBUG) send(stdout, "println", "supervisor::result");
    check_factors(vec);
    if (--m_left == 0) {
      destroy("done");
    }
  }

  private void check_factors(Vector<Long> vec) {
    if (vec.size() == 2 && vec.get(0) == mixed_case.s_factor1
        && vec.get(1) == mixed_case.s_factor2) {
      //send(stdout, "println", "OKAY");
    } else {
      send(stdout, "println", "ERROR");
    }
  }
}
