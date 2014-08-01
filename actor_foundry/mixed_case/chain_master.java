package osl.examples.caf_benches;
 
import osl.manager.*;
import osl.util.*;
import osl.manager.annotations.message;

public class chain_master extends Actor {
  private static final boolean DEBUG         = false;
  private static final long serialVersionUID = 4578541365981326142L;
  public  static final      String _CLASS    =
                              "osl.examples.caf_benches.chain_master";
  public  static ActorName chain_master_instance = null;

  private int       m_iteration;
  private ActorName m_mc;
  private ActorName m_next;
  private ActorName m_factorizer;

  private int       m_ring_size;
  private int       m_initial_token;
  private int       m_repetitions;

  @message
  public void init(ActorName msgcollector, Integer ring_size,
                   Integer initial_token, Integer repetitions) 
                   throws RemoteCodeException {
    if (DEBUG) send(stdout, "println", "chain_master::init");
    chain_master_instance = self();
    m_iteration     = 0;
    m_mc            = msgcollector;
    m_factorizer    = create(worker._CLASS);
    send(m_factorizer, "init", m_mc);
    m_ring_size     = ring_size;
    m_initial_token = initial_token;
    m_repetitions   = repetitions;
    new_ring(ring_size, initial_token);
  }

  @message
  public void token(Integer y) throws RemoteCodeException {
    if (DEBUG) send(stdout, "println", "chain_master::token");
    if (y == 0) {
      if (++m_iteration < m_repetitions) {
        new_ring(m_ring_size, m_initial_token);
      } else {
        send(m_factorizer, "done");
        send(m_mc, "masterdone");
      }
    } else {
      send(m_next, "token", new Integer(y.intValue() - 1));
    }
  }

  private void new_ring(int ring_size, int initial_token_value)
                        throws RemoteCodeException {
    send(m_factorizer, "calc", mixed_case.s_task_n);
    ActorName temp = self(); // temp...
    for (int i = 1; i < ring_size; ++i) {
      m_next = create(chain_link._CLASS);
      send(m_next, "init", temp);
      temp = m_next;
    }
    send(m_next, "token", new Integer(initial_token_value));
  }
}
