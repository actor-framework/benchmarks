package osl.examples.caf_benches;

import osl.manager.*;
import osl.util.*;
import osl.manager.annotations.message;

public class mailbox_performance extends Actor {
  private static final long serialVersionUID = 4272570623748766142L;
  public  static final String _CLASS =
                        "osl.examples.caf_benches.mailbox_performance";

  @message
  public void boot(String in) throws RemoteCodeException {
    String[] splitted  = in.split("_");
    Integer num_sender = Integer.parseInt(splitted[1]);
    Long    num_msgs   = Long.parseLong(splitted[2]);
    ActorName testee   = create(receiver._CLASS);
    send(testee, "init", new Long(num_sender.intValue() * num_msgs.intValue()));
    for (long i = 0; i < num_sender; ++i) {
      send(create(sender._CLASS), "run", testee, num_msgs);
    }
  }
}
