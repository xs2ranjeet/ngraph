// ----------------------------------------------------------------------------
// Copyright 2017 Nervana Systems Inc.
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// ----------------------------------------------------------------------------

#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "gtest/gtest.h"

#include "ngraph/ngraph.hpp"
#include "ngraph/topological_sort.hpp"
#include "ngraph/visualize.hpp"

using namespace std;
using namespace ngraph;

static bool validate_list(const vector<Node*>& nodes)
{
    bool rc = true;
    for (auto it=nodes.rbegin(); it!=nodes.rend(); it++)
    {
        Node* node = *it;
        auto node_tmp = *it;
        auto dependencies_tmp = node_tmp->arguments();
        vector<Node*> dependencies;
        for (shared_ptr<Node> n : dependencies_tmp)
        {
            dependencies.push_back(n.get());
        }
        auto tmp = it+1;
        for (; tmp!=nodes.rend(); tmp++)
        {
            auto dep_tmp = *tmp;
            auto found = find(dependencies.begin(), dependencies.end(), dep_tmp);
            if (found != dependencies.end())
            {
                dependencies.erase(found);
            }
        }
        if (dependencies.size() > 0)
        {
            rc = false;
        }
    }
    return rc;
}

TEST(topological_sort, basic)
{
    vector<shared_ptr<Parameter>> args;
    for (int i=0; i<10; i++)
    {
        auto arg = op::parameter(element::Float::type, {1});
        ASSERT_NE(nullptr, arg);
        args.push_back(arg);
    }

    auto t0 = op::add(args[0], args[1]);
    ASSERT_NE(nullptr, t0);
    auto t1 = op::dot(t0, args[2]);
    ASSERT_NE(nullptr, t1);
    auto t2 = op::multiply(t0, args[3]);
    ASSERT_NE(nullptr, t2);

    auto t3 = op::add(t1, args[4]);
    ASSERT_NE(nullptr, t2);
    auto t4 = op::add(t2, args[5]);
    ASSERT_NE(nullptr, t3);

    Node::ptr r0 = op::add(t3, t4);
    ASSERT_NE(nullptr, r0);

    auto f0 = op::function(r0, args);
    ASSERT_NE(nullptr, f0);

    ASSERT_EQ(2, r0->arguments().size());
    auto op_r0 = static_pointer_cast<Op>(r0);

    Visualize vz;
    vz.add(r0);
    vz.save_dot("test.png");
    TopologicalSort ts;
    ts.process(r0);
    auto sorted_list = ts.get_sorted_list();

    EXPECT_TRUE(validate_list(sorted_list));
}