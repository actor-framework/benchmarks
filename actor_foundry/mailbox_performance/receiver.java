package osl.examples.caf_benches;

import osl.manager.*;
import osl.util.*;
import osl.manager.annotations.message;

public class receiver extends Actor {
  private static final long serialVersionUID = 4578965423751326142L;
//  public  static final String _CLASS =
//                       "osl.examples.caf_benches.receiver";
  private              long   m_max;
  private              long   m_value;

  public receiver(Long max) {
    super();
    m_max   = max.intValue();
    m_value = 0;
  }

  @message
  public void msg() {
    if (++m_value == m_max) {
      destroy("done");
    }
  }
}
