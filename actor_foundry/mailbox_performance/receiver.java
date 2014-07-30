package osl.examples.caf_benches.mailbox_performance;

import osl.manager.*;
import osl.util.*;
import osl.manager.annotations.message;

public class receiver extends Actor {
  private static final long serialVersionUID = 4578965423751326142L;
  public  static final String _CLASS =
                       "osl.examples.caf_benches.mailbox_performance.receiver";
  private              long   m_max;
  private              long   m_value;

  @message
  public void init(Long max) {
    m_max   = max.intValue();
    m_value = 0;
  }

  @message
  public void msg() {
    if (++m_value == m_max) {
      // done... nothing...
    }
  }
}
