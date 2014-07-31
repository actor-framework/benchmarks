package osl.examples.caf_benches.mixed_case;
 
import osl.manager.*;
import osl.util.*;
import osl.manager.annotations.message;

public class chain_link extends Actor {
  private static final boolean DEBUG         = true;
  private static final long serialVersionUID = 4578547894020479372L;
  public  static final      String _CLASS    =
                              "osl.examples.caf_benches.mixed_case.chain_link";

  private ActorName m_next;
  
  @message
  public void init(ActorName next) {
    if (DEBUG) send(stdout, "println", "chain_link::init");
    m_next = next;
  }

  @message
  public void token(Integer v) {
    if (DEBUG) send(stdout, "println", "chain_link::token");
    if (chain_master.chain_master_instance == m_next) send(stdout, "println", "I should send to master... " + m_next.toString() + " V: " + v.toString());
    send(m_next, "token", v);
    if (v == 0) {
      destroy("done");
    }
  }
}
