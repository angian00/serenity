/*
 * Copyright (c) 2020, the SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/StringView.h>
#include <AK/Types.h>
#include <LibJS/SourceCode.h>

namespace JS {

struct Position {
    size_t line { 0 };
    size_t column { 0 };
    size_t offset { 0 };
};

struct SourceRange {
    [[nodiscard]] bool contains(Position const& position) const { return position.offset <= end.offset && position.offset >= start.offset; }

    NonnullRefPtr<SourceCode> code;
    Position start;
    Position end;

    String const& filename() const;
};

}
