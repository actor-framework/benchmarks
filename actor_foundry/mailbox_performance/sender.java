package osl.examples.caf_benches;

import osl.manager.*;
import osl.util.*;
import osl.manager.annotations.message;

public class sender extends Actor {
  private static final long serialVersionUID = 4273990623755478312L;
  public static final String _CLASS =
                      "osl.examples.caf_benches.sender";
  @message
  public void run(ActorName whom, Long count) {
    for (long i = 0; i < count; ++i) {
      send(whom, "msg");
    }
  }
}
