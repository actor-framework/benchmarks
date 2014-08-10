package osl.examples.caf_benches;
 
import osl.manager.*;
import osl.util.*;
import osl.manager.annotations.message;

public class chain_link extends Actor {
  private static final long serialVersionUID = 4578547894020479372L;
  public  static final      String _CLASS    =
                              "osl.examples.caf_benches.chain_link";

  private ActorName m_next;

  public chain_link(ActorName next) {
    super();
    m_next = next;
  }

  @message
  public void token(Integer v) {
    send(m_next, "token", v);
    if (v == 0) {
      destroy("done");
    }
  }
}
