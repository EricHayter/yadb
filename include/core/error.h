#pragma once

/* There's lot of spots where errors can happen in different parts of the stack
 * this just defines a simple interface for errors with a simple string
 * representation of the error.
 */
#include <memory>
#include <string>

namespace yadb {

class Error {
public:
    using Ptr = std::shared_ptr<Error>;

    virtual ~Error() = default;
    virtual std::string what() const = 0;
};

}
