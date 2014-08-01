// java -cp lib/foundry-1.0.jar:classes osl.foundry.FoundryStart  osl.examples.caf_benches.mixed_case.mainactor boot "_1_10_30_1"
package osl.examples.caf_benches;
 
import osl.manager.*;
import osl.util.*;
import osl.manager.annotations.message;

import java.util.Vector;

public class mixed_case extends Actor {
  private static final boolean DEBUG         = false;
  private static final long serialVersionUID = 4277890623751326142L;
  public  static final      String _CLASS    =
                              "osl.examples.caf_benches.mixed_case";
  public  static final long s_factor1        = 86028157;
  public  static final long s_factor2        = 329545133;
  public  static final long s_task_n         = s_factor1 * s_factor2;

  private static ActorName m_mainactor           = null;
  private        int       m_num_rings           = 0;
  private        int       m_ring_size           = 0;
  private        int       m_initial_token_value = 0;
  private        int       m_repetitions         = 0;

  /* the main actor store  */
  public static ActorName getMainActor() {
    return m_mainactor;
  }

  @message
  public void boot(String in) throws RemoteCodeException {
    m_mainactor           = self();
    String[] args         = in.split("_");
    m_num_rings           = Integer.parseInt(args[1]);
    m_ring_size           = Integer.parseInt(args[2]);
    m_initial_token_value = Integer.parseInt(args[3]);
    m_repetitions         = Integer.parseInt(args[4]);
    if (DEBUG) {
      send(stdout, "println", "m_num_rings: "     + m_num_rings);
      send(stdout, "println", "m_ring_size: "     + m_ring_size);
      send(stdout, "println", "m_initial_token: " + m_initial_token_value);
      send(stdout, "println", "m_repetitions: "   + m_repetitions);
    }
    int num_msgs = m_num_rings + (m_num_rings * m_repetitions);
    ActorName sv = create(supervisor._CLASS);
    send(sv, "init", num_msgs);
    Vector<ActorName> masters = new Vector<ActorName>();
    for (int i = 0; i < m_num_rings; ++i) {
      masters.add(create(chain_master._CLASS));
      send(masters.lastElement(), "init", sv, m_ring_size,
           m_initial_token_value, m_repetitions);
    }
  }
}
