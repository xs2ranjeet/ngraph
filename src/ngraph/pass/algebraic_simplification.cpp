/*******************************************************************************
* Copyright 2017-2018 Intel Corporation
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/

#include <memory>
#include <set>

#include "algebraic_simplification.hpp"
#include "ngraph/graph_util.hpp"
#include "ngraph/log.hpp"
#include "ngraph/op/add.hpp"
#include "ngraph/op/broadcast.hpp"
#include "ngraph/op/constant.hpp"
#include "ngraph/op/multiply.hpp"
#include "ngraph/op/product.hpp"
#include "ngraph/op/sum.hpp"
#include "ngraph/pattern/matcher.hpp"

using namespace ngraph;

#define TI(x) std::type_index(typeid(x))

static std::shared_ptr<Node> canonicalize_constant(std::shared_ptr<Node> cnst, double val)
{
    return cnst->get_shape().size() > 0
               ? cnst
               : op::Constant::create(cnst->get_element_type(), Shape{}, {val});
}

template <typename T>
static std::shared_ptr<pattern::Matcher>
    create_binary_matcher(std::shared_ptr<pattern::op::Label> label,
                          std::shared_ptr<pattern::op::Label> const_label)
{
    auto bcst_pred = [](std::shared_ptr<Node> n) {
        return std::dynamic_pointer_cast<op::Broadcast>(n) != nullptr;
    };
    auto bcst = std::make_shared<pattern::op::Any>(const_label, bcst_pred);
    auto matcher = std::make_shared<pattern::Matcher>(std::make_shared<T>(label, bcst), nullptr);
    return matcher;
}

static bool simplify_multiply(std::shared_ptr<Node> n)
{
    auto iconst = ngraph::make_zero(element::i32, Shape{});
    auto label = std::make_shared<pattern::op::Label>(iconst);
    auto const_label = std::make_shared<pattern::op::Label>(iconst, nullptr, NodeVector{iconst});
    auto matcher = create_binary_matcher<op::Multiply>(label, const_label);

    if (matcher->match(n))
    {
        auto pattern_map = matcher->get_pattern_map();
        auto x = pattern_map[label];
        auto cnst = pattern_map[const_label];

        auto can_const = canonicalize_constant(cnst, 0);
        if (ngraph::is_zero(can_const))
        {
            ngraph::replace_node(n, can_const);
            return true;
        }

        can_const = canonicalize_constant(cnst, 1);
        if (ngraph::is_one(can_const))
        {
            ngraph::replace_node(n, label);
            return true;
        }
    }
    return false;
}

static bool simplify_add(std::shared_ptr<Node> n)
{
    auto iconst = ngraph::make_zero(element::i32, Shape{});
    auto label = std::make_shared<pattern::op::Label>(iconst);
    auto const_label = std::make_shared<pattern::op::Label>(iconst, nullptr, NodeVector{iconst});
    auto matcher = create_binary_matcher<op::Multiply>(label, const_label);

    if (matcher->match(n))
    {
        auto pattern_map = matcher->get_pattern_map();
        auto x = pattern_map[label];
        auto cnst = pattern_map[const_label];

        auto can_const = canonicalize_constant(cnst, 0);
        if (ngraph::is_zero(can_const))
        {
            ngraph::replace_node(n, label);
            return true;
        }
    }
    return false;
}

static std::unordered_map<std::type_index, std::function<bool(std::shared_ptr<Node>)>>
    initialize_const_values_to_ops()
{
    return std::unordered_map<std::type_index, std::function<bool(std::shared_ptr<Node>)>>({
        {TI(op::Add), simplify_add}, {TI(op::Multiply), simplify_multiply},
    });
}

static std::unordered_map<std::type_index, std::function<bool(std::shared_ptr<Node>)>>
    ops_to_const_values = initialize_const_values_to_ops();

bool ngraph::pass::AlgebraicSimplification::run_on_function(std::shared_ptr<ngraph::Function> f)
{
    bool replaced = false;
    for (auto n : f->get_ordered_ops())
    {
        if (n->is_output() || n->is_parameter())
        {
            continue;
        }

        const Node& node = *n;
        auto eh = ops_to_const_values.find(TI(node));
        if (eh == ops_to_const_values.end())
        {
            continue;
        }

        replaced = replaced || eh->second(n);
    }
    return replaced;
}
