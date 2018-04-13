#include <iostream>

//////////////
// C++ code //
//////////////
struct A
{
  int i;
  int j;

  A() {i=1; j=2; std::cout << "class A created\n";}
  void dump() {std::cout << "class A dumped: " << i << ":" << j << std::endl;}
  ~A() {std::cout << "class A destroyed\n";}
};

extern "C" {
  // this is the C code interface to the class A
  static void *createA (void)
  {
    // create a handle to the A class
    return (void *)(new A);
  }
  static void dumpA (void *thisPtr)
  {
    // call A->dump ()
    if (thisPtr != NULL) // I'm an anal retentive programmer
    {
      A *classPtr = static_cast<A *>(thisPtr);
      classPtr->dump ();
    }
  }
  static void *deleteA (void *thisPtr)
  {
    // destroy the A class
    if (thisPtr != NULL)
    {
      delete (static_cast<A *>(thisPtr));
    }
  }
}

////////////////////////////////////
// this can be compiled as C code //
////////////////////////////////////
int main (int argc, char **argv)
{
  void *handle = createA();

  dumpA (handle);
  deleteA (handle);
  
  return 0;
}
