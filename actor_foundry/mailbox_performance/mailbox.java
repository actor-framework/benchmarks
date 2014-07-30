package osl.examples.caf_benches.mailbox_performance;

// bei error:
// 1.) ant clean
// 2.) ant compile
// 3.) ant codegen
// 4.) ant weave -k
//java -cp lib/foundry-1.0.jar:classes osl.foundry.FoundryStart osl.examples.caf_benches.mailbox_performance.mailbox boot 20 1000
//java -cp lib/foundry-1.0.jar:classes osl.foundry.FoundryStart osl.examples.caf_benches.mailbox_performance.mailbox boot "_20_1000000_"

import osl.manager.*;
import osl.util.*;
import osl.manager.annotations.message;

public class mailbox extends Actor {
  private static final long serialVersionUID = 4272570623748766142L;
  public  static final String _CLASS =
                        "osl.examples.caf_benches.mailbox_performance.mailbox";

  @message
  public void boot(String in) throws RemoteCodeException {
    // _20_1000000_ = 20 senders, 1000000 msgs
    //send(stdout, "println", "in: " + in);
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
