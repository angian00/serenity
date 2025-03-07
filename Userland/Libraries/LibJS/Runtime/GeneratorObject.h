/*
 * Copyright (c) 2021, Ali Mohammad Pur <mpfard@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibJS/Bytecode/Interpreter.h>
#include <LibJS/Runtime/ECMAScriptFunctionObject.h>
#include <LibJS/Runtime/Object.h>

namespace JS {

class GeneratorObject final : public Object {
    JS_OBJECT(GeneratorObject, Object);

public:
    static ThrowCompletionOr<GeneratorObject*> create(Realm&, Value, ECMAScriptFunctionObject*, ExecutionContext, Bytecode::RegisterWindow);
    virtual void initialize(Realm&) override;
    virtual ~GeneratorObject() override = default;
    void visit_edges(Cell::Visitor&) override;

    ThrowCompletionOr<Value> resume(VM&, Value value, Optional<String> generator_brand);
    ThrowCompletionOr<Value> resume_abrupt(VM&, JS::Completion abrupt_completion, Optional<String> generator_brand);

private:
    GeneratorObject(Realm&, Object& prototype, ExecutionContext);

    enum class GeneratorState {
        SuspendedStart,
        SuspendedYield,
        Executing,
        Completed,
    };

    ThrowCompletionOr<GeneratorState> validate(VM&, Optional<String> const& generator_brand);
    ThrowCompletionOr<Value> execute(VM&, JS::Completion const& completion);

    ExecutionContext m_execution_context;
    ECMAScriptFunctionObject* m_generating_function { nullptr };
    Value m_previous_value;
    Optional<Bytecode::RegisterWindow> m_frame;
    GeneratorState m_generator_state { GeneratorState::SuspendedStart };
    Optional<String> m_generator_brand;
};

}
