#include <iostream>

#include "charm++.h"
#include "creation.decl.h"

struct main : public CBase_main {
   void usage() {
     using namespace std;
     cout << endl << endl
          << "invalid argument." << endl 
          << "./creation (1-9)*" << endl
          << " creates 2^POW actors" << endl << endl;
   }   
   main(CkArgMsg* m) {
     using namespace std;
     if (m->argc != 2) {
       usage();
       CkExit();
     }   
     int s_num = atoi(m->argv[1]);
     CProxy_testee::ckNew(s_num, true, CProxy_testee());
   }   
};

class testee : public CBase_testee {
  testee_SDAG_CODE

  CProxy_testee m_parent;
  bool          m_is_root;
public:
  
  testee(int s_num, bool is_root, CProxy_testee parent):
  m_parent(parent), m_is_root(is_root) {
    calc(s_num); 
  } 

  void respond(int val, int val2) {
    if(m_is_root) {
      if(val == val2) {
        CkExit();
      } else {
        using namespace std;
        cout << "Result is:" << val << " + "
             << val2 << endl;
        CkExit();
      }
    } else {
      m_parent.response(1 + val + val2);
      delete this;
    }
  }
};

#include "creation.def.h"
