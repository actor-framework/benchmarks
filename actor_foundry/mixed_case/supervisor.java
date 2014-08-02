package osl.examples.caf_benches;
 
import osl.manager.*;
import osl.util.*;
import osl.manager.annotations.message;

import java.util.Vector;

public class supervisor extends Actor {
  private static final long serialVersionUID = 4521476353751326142L;
  public  static final      String _CLASS    =
                              "osl.examples.caf_benches.supervisor";

  private int m_left = 0;

  @message
  public void init(Integer left) {
    m_left = left.intValue();
  }
  
  @message
  public void masterdone() {
    if (--m_left == 0) {
      destroy("done");
    }
  }

  @message
  public void result(Vector<Long> vec) {
    check_factors(vec);
    if (--m_left == 0) {
      destroy("done");
    }
  }

  private void check_factors(Vector<Long> vec) {
    if (vec.size() != 2 || vec.get(0) != mixed_case.s_factor1
        || vec.get(1) != mixed_case.s_factor2) {
      send(stdout, "println", "ERROR");
    }
  }
}
