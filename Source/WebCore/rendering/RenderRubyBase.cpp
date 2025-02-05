/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "RenderRubyBase.h"
#include "RenderRubyRun.h"
#include "RenderRubyText.h"

namespace WebCore {

RenderRubyBase::RenderRubyBase(Document& document, RenderStyle&& style)
    : RenderBlockFlow(document, WTFMove(style))
    , m_initialOffset(0)
    , m_isAfterExpansion(true)
{
    setInline(false);
}

RenderRubyBase::~RenderRubyBase()
{
}

bool RenderRubyBase::isChildAllowed(const RenderObject& child, const RenderStyle&) const
{
    return child.isInline();
}

void RenderRubyBase::moveChildren(RenderRubyBase* toBase, RenderObject* beforeChild)
{
    // This function removes all children that are before (!) beforeChild
    // and appends them to toBase.
    ASSERT_ARG(toBase, toBase);

    if (beforeChild && beforeChild->parent() != this)
        beforeChild = splitAnonymousBoxesAroundChild(beforeChild);

    if (childrenInline())
        moveInlineChildren(toBase, beforeChild);
    else
        moveBlockChildren(toBase, beforeChild);

    setNeedsLayoutAndPrefWidthsRecalc();
    toBase->setNeedsLayoutAndPrefWidthsRecalc();
}

void RenderRubyBase::mergeChildrenWithBase(RenderRubyBase& toBlock)
{
    moveChildren(&toBlock);
    addFloatsToNewParent(toBlock);
}

void RenderRubyBase::moveInlineChildren(RenderRubyBase* toBase, RenderObject* beforeChild)
{
    ASSERT(childrenInline());
    ASSERT_ARG(toBase, toBase);

    if (!firstChild())
        return;

    RenderBlock* toBlock;
    if (toBase->childrenInline()) {
        // The standard and easy case: move the children into the target base
        toBlock = toBase;
    } else {
        // We need to wrap the inline objects into an anonymous block.
        // If toBase has a suitable block, we re-use it, otherwise create a new one.
        RenderObject* lastChild = toBase->lastChild();
        if (lastChild && lastChild->isAnonymousBlock() && lastChild->childrenInline())
            toBlock = downcast<RenderBlock>(lastChild);
        else {
            auto newToBlock = toBase->createAnonymousBlock();
            toBlock = newToBlock.get();
            toBase->insertChildInternal(WTFMove(newToBlock), nullptr, NotifyChildren);
        }
    }
    // Move our inline children into the target block we determined above.
    moveChildrenTo(toBlock, firstChild(), beforeChild);
}

void RenderRubyBase::moveBlockChildren(RenderRubyBase* toBase, RenderObject* beforeChild)
{
    ASSERT(!childrenInline());
    ASSERT_ARG(toBase, toBase);

    if (!firstChild())
        return;

    if (toBase->childrenInline())
        toBase->makeChildrenNonInline();

    // If an anonymous block would be put next to another such block, then merge those.
    RenderObject* firstChildHere = firstChild();
    RenderObject* lastChildThere = toBase->lastChild();
    if (firstChildHere->isAnonymousBlock() && firstChildHere->childrenInline() 
            && lastChildThere && lastChildThere->isAnonymousBlock() && lastChildThere->childrenInline()) {            
        RenderBlock* anonBlockHere = downcast<RenderBlock>(firstChildHere);
        RenderBlock* anonBlockThere = downcast<RenderBlock>(lastChildThere);
        anonBlockHere->moveAllChildrenTo(anonBlockThere, true);
        anonBlockHere->deleteLines();
        anonBlockHere->removeFromParentAndDestroy();
    }
    // Move all remaining children normally.
    moveChildrenTo(toBase, firstChild(), beforeChild);
}

RenderRubyRun* RenderRubyBase::rubyRun() const
{
    ASSERT(parent());
    return downcast<RenderRubyRun>(parent());
}

ETextAlign RenderRubyBase::textAlignmentForLine(bool /* endsWithSoftBreak */) const
{
    return JUSTIFY;
}

void RenderRubyBase::adjustInlineDirectionLineBounds(int expansionOpportunityCount, float& logicalLeft, float& logicalWidth) const
{
    if (rubyRun()->hasOverrideLogicalContentWidth() && firstRootBox() && !firstRootBox()->nextRootBox()) {
        logicalLeft += m_initialOffset;
        logicalWidth -= 2 * m_initialOffset;
        return;
    }

    LayoutUnit maxPreferredLogicalWidth = rubyRun() && rubyRun()->hasOverrideLogicalContentWidth() ? rubyRun()->overrideLogicalContentWidth() : this->maxPreferredLogicalWidth();
    if (maxPreferredLogicalWidth >= logicalWidth)
        return;

    // Inset the ruby base by half the inter-ideograph expansion amount.
    float inset = (logicalWidth - maxPreferredLogicalWidth) / (expansionOpportunityCount + 1);

    logicalLeft += inset / 2;
    logicalWidth -= inset;
}

void RenderRubyBase::cachePriorCharactersIfNeeded(const LazyLineBreakIterator& lineBreakIterator)
{
    auto* run = rubyRun();
    if (run)
        run->setCachedPriorCharacters(lineBreakIterator.lastCharacter(), lineBreakIterator.secondToLastCharacter());
}

} // namespace WebCore
