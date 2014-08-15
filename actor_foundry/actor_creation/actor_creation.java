package osl.examples.caf_benches;

import osl.manager.*;
import osl.util.*;
import osl.manager.annotations.message;

public class actor_creation extends Actor {
  private static final long serialVersionUID = 4273990623751326142L;

  private static int       s_num    = 0;
  private static ActorName master   = null;  /* master actor */
  private static int       m_reach  = 0;
  private        ActorName m_parent = null;
  private        int       m_r1     = 0;
  private        int       m_r2     = 0;

  @message
  public void boot(String in) throws RemoteCodeException {
    s_num  = Integer.parseInt(in.split("_")[1]);
    master = self();
    send(master, "spread", master, s_num);
  }

  @message
  public void spread(ActorName parent, Integer x) throws RemoteCodeException {
    m_parent = parent;
    if(x == 1) {
      send(parent, "result", 1);
      //destroy("done");
    } else {
      final Integer msg = x.intValue() - 1;
      ActorName child_1 = create(actor_creation.class);
      ActorName child_2 = create(actor_creation.class);
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
          send(m_parent, "result", 1 + m_r1 + m_r2);
          return;
        }
        final int result  = 2 + m_r1 + m_r2;
        final int expected = (1 << s_num);
        if (result != expected) {
          send(stdout, "println", "expected: " + expected +
               " found: " + result);
        }
      } else  {
        send(m_parent, "result", 1 + m_r1 + m_r2);
      }
    }
  }
}

