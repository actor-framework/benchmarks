//java -cp lib/foundry-1.0.jar:classes osl.foundry.FoundryStart osl.examples.caf_benches.actor_creation.testactor boot 20
package osl.examples.caf_benches.actor_creation;

import osl.manager.*;
import osl.util.*;
import osl.manager.annotations.message;

public class testactor extends Actor {
  private static final long serialVersionUID = 4273990623751326142L;
  private static final String _CLASS =
                           "osl.examples.caf_benches.actor_creation.testactor";
  private static int       s_num    = 0;
  private static ActorName master   = null;  /* master actor */
  private static int       m_reach  = 0;
  private        ActorName m_parent = null;
  private        int       m_r1     = 0;
  private        int       m_r2     = 0;

  @message
  public void boot(Integer num) throws RemoteCodeException {
    s_num  = num.intValue();
    master = self();
    send(master, "spread", master, num);
  }

  @message
  public void spread(ActorName parent, Integer x) throws RemoteCodeException {
    m_parent = parent;
    if(x == 1) {
      send(parent, "result", 1);
    } else {
      final Integer msg = new Integer(x.intValue() - 1);
      ActorName child_1 = create(_CLASS);
      ActorName child_2 = create(_CLASS);
      send(child_1, "spread", self(), msg);
      send(child_2, "spread", self(), msg);
    }
  }

  @message
  public void result(Integer r) {
    if      (m_r1 == 0) m_r1 = r.intValue();
    else if (m_r2 == 0) m_r2 = r.intValue();
    if (m_r1 != 0 && m_r2 != 0) {
      if (m_parent == master) {
        if (m_reach++ <= 2) {
          /* 2 child actors refer to master actor, but we just wanna validate
             if we reached the master again
           */
          send(m_parent, "result", new Integer(1 + m_r1 + m_r2));
          return;
        }
        final int result  = 2 + m_r1 + m_r2;
        final int expected = (1 << s_num);
        if (result != expected) {
          send(stdout, "println", "expected: " + expected +
               " found: " + result);
        }
      } else  {
        send(m_parent, "result", new Integer(1 + m_r1 + m_r2));
      }
    }
  }
}

