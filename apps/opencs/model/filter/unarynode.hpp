#ifndef CSM_FILTER_UNARIYNODE_H
#define CSM_FILTER_UNARIYNODE_H

#include <memory>

#include "node.hpp"

namespace CSMFilter
{
    class UnaryNode : public Node
    {
            std::auto_ptr<Node> mChild;

        public:

            UnaryNode (std::auto_ptr<Node> child);

            const Node& getChild() const;

            Node& getChild();

            virtual std::vector<std::string> getReferencedFilters() const;
            ///< Return a list of filters that are used by this node (and must be passed as
            /// otherFilters when calling test).

            virtual std::vector<int> getReferencedColumns() const;
            ///< Return a list of the IDs of the columns referenced by this node. The column mapping
            /// passed into test as columns must contain all columns listed here.

            virtual bool isSimple() const;
            ///< \return Can this filter be displayed in simple mode.

            virtual bool hasUserValue() const;
    };
}

#endif
