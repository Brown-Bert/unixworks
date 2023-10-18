#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <iostream>

#include "copyfile.h"
using namespace std;
std::string sourceFileName =
    "/home/yzx/vscode/workspace/unixworks/work2/src/test.txt";
std::string destination =
    "/home/yzx/vscode/workspace/unixworks/work2/src/testcopy.txt";

TEST(COMPARETEST, COMPARE) {
  EXPECT_EQ(true, compare(sourceFileName, destination));
}