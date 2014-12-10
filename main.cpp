/**
 * Copyright Flexible Software Solutions S.L. 2014
 **/

#include "FlexVMAgent.hpp"

int main(int argc, char * argv[]) {
    flexvm::FlexVMAgent agent(argc, argv);
    return agent.run();
}
