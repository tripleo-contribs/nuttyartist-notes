#include "blockmodel.h"

BlockModel::BlockModel(QObject *parent)
    : QAbstractListModel{ parent },
      m_sourceDocument(this),
      m_blockList({}),
      m_htmlMetaDataStart(QStringLiteral("<html><head><meta name=\"qrichtext\" content=\"1\" "
                                         "/><meta charset=\"utf-8\" /></head><body>")),
      m_htmlMetaDataEnd(QStringLiteral("</body></html>")),
      m_tabLengthInSpaces(4),
      m_textLineHeightInPercentage(125),
      m_blockIndexToFocusOn(0),
      m_undoStack({}),
      m_redoStack({})
{
    Q_UNUSED(parent);
    m_clipboard = QGuiApplication::clipboard();
}

int BlockModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_blockList.length();
}

QVariant BlockModel::data(const QModelIndex &index, int role) const
{
    if (index.isValid() && index.row() >= 0 && index.row() < m_blockList.length()) {
        QSharedPointer<BlockInfo> blockInfo = m_blockList[index.row()];

        switch ((Role)role) {
        case BlockTextHtmlRole:
            return blockInfo->textHtml();
        case BlockTextPlainTextRole:
            return blockInfo->textPlainText();
        case BlockDelimiterRole:
            return blockInfo->blockDelimiter();
        case BlockTypeRole:
            return static_cast<int>(blockInfo->blockType());
        case BlockLineStartPosRole:
            return blockInfo->lineStartPos();
        case BlockLineEndPosRole:
            return blockInfo->lineEndPos();
        case BlockTotalIndentLengthRole:
            return blockInfo->totalIndentLength();
        case BlockIndentLevelRole:
            return blockInfo->indentLevel();
        case BlockChildrenRole:
            return QVariant::fromValue(blockInfo->children());
        case BlockMetaData:
            return QVariant(blockInfo->metaData());
        }
    }

    return {};
}

QHash<int, QByteArray> BlockModel::roleNames() const
{
    QHash<int, QByteArray> result;

    result[BlockTextHtmlRole] = "blockTextHtml";
    result[BlockTextPlainTextRole] = "blockTextPlainText";
    result[BlockTypeRole] = "blockType";
    result[BlockLineStartPosRole] = "blockLineStartPos";
    result[BlockLineEndPosRole] = "blockLineEndPos";
    result[BlockTotalIndentLengthRole] = "blockTotalIndentLength";
    result[BlockIndentLevelRole] = "blockIndentLevel";
    result[BlockDelimiterRole] = "blockDelimiter";
    result[BlockChildrenRole] = "blockChildren";
    result[BlockMetaData] = "blockMetaData";

    return result;
}

QTextDocument *BlockModel::sourceDocument()
{
    return &m_sourceDocument;
}

void BlockModel::processOutput(const MD_CHAR *output, MD_SIZE size, void *userdata)
{
    QByteArray *buffer = reinterpret_cast<QByteArray *>(userdata);
    buffer->append(output, size);
}

QString BlockModel::markdownToHtml(const QString &markdown)
{
    QByteArray md = markdown.toUtf8();
    QByteArray html;

    unsigned parser_flags = MD_FLAG_STRIKETHROUGH | MD_FLAG_NOINDENTEDCODEBLOCKS;

    if (md_html((MD_CHAR *)md.data(), md.size(), &processOutput, (void *)&html, parser_flags, 0)
        == 0)
        return QString::fromUtf8(html);
    else
        return QString(); // or throw an exception
}

// TODO: this function should be in BlockInfo not here
unsigned int BlockModel::calculateTotalIndentLength(const QString &str,
                                                    QSharedPointer<BlockInfo> &blockInfo)
{
    unsigned int totalIndentLength = 0;
    QString indentedString = "";
    for (auto &c : str) {
        if (c == '\t') {
            totalIndentLength += m_tabLengthInSpaces;
            indentedString += c;
        } else if (c == ' ') {
            totalIndentLength += 1;
            indentedString += c;
        } else {
            break;
        }
    }
    blockInfo->setIndentedString(indentedString); // TODO: this should not be here
    return totalIndentLength;
}

// TODO: this function should be inside BlockInfo, not BlockModel
void BlockModel::updateBlockText(QSharedPointer<BlockInfo> &blockInfo, const QString &plainText,
                                 unsigned int lineStartPos, unsigned int lineEndPos)
{
    // Plain text
    blockInfo->setTextPlainText(plainText);

    // HTML
    // We clean the plain text string from:
    // 1. Indentation at the start of the string
    // 2. Delimeter
    QString htmlOutput = plainText.mid(blockInfo->indentedString().length()
                                       + blockInfo->blockDelimiter().length());

    //    qDebug() << "BlockType (in updateBlockText): " << blockInfo->blockType();
    //    qDebug() << "htmlOutput 1: " << htmlOutput;
    htmlOutput = m_htmlMetaDataStart
            + (htmlOutput.length() < 2 ? htmlOutput : markdownToHtml(htmlOutput))
            + m_htmlMetaDataEnd;
    //    qDebug() << "htmlOutput 2: " << htmlOutput;
    if (blockInfo->blockType() != BlockInfo::BlockType::Heading) {
        htmlOutput.replace(
                "<p>",
                QStringLiteral("<p style=\"line-height:%1%;\">").arg(m_textLineHeightInPercentage));
        if (!htmlOutput.contains("<p>"))
            htmlOutput.replace("<body>",
                               QStringLiteral("<body style=\"line-height:%1%;\">")
                                       .arg(m_textLineHeightInPercentage));
        //        qDebug() << "htmlOutput 3: " << htmlOutput;
    }
    blockInfo->setTextHtml(htmlOutput);

    // Lines
    blockInfo->setLineStartPos(lineStartPos);
    blockInfo->setLineEndPos(lineEndPos);
}

void BlockModel::determineBlockIndentAndParentChildRelationship(
        QSharedPointer<BlockInfo> &blockInfo, int positionToStartSearchFrom)
{
    //    qDebug() << "In determineBlockIndentAndParentChildRelationship";
    if (positionToStartSearchFrom < 1 || positionToStartSearchFrom > m_blockList.length() - 1)
        return;

    if (blockInfo->totalIndentLength() == 0) {
        if (blockInfo->parent() != nullptr)
            blockInfo->parent()->removeChild(blockInfo);
        //        blockInfo->setParent(nullptr);
        return;
    }

    //    qDebug() << "here 0";

    // We traverse the list backwards to check if this block has a parent
    for (int i = positionToStartSearchFrom; i >= 0; i--) {
        QSharedPointer<BlockInfo> previousBlock = m_blockList[i];
        //        qDebug() << "previousBlock->totalIndentLength(): " <<
        //        previousBlock->totalIndentLength();

        // If previousBlock has a smaller total indent length than the current line/block
        // We assign it has a parent for the current block
        if (previousBlock->totalIndentLength() < blockInfo->totalIndentLength()) {

            //            qDebug() << "here 1";

            // If parent is changing, we remove this block from its children
            if (blockInfo->parent() != nullptr) { //&& blockInfo->parent() != previousBlock) {
                blockInfo->parent()->removeChild(blockInfo);
                //                qDebug() << "here 2";
                //                qDebug() << "parent text: " <<
                //                blockInfo->parent()->textPlainText();
            }

            blockInfo->setIndentLevel(previousBlock->indentLevel() + 1);
            //            qDebug() << "previousBlock->indentLevel() + 1: " <<
            //            previousBlock->indentLevel() + 1;
            blockInfo->setParent(previousBlock);
            previousBlock->addChild(blockInfo);
            //            previousBlock->addChild(blockInfo);
            break;
        }
    }
}

void BlockModel::updateBlockUsingPlainText(QSharedPointer<BlockInfo> &blockInfo,
                                           unsigned int blockIndex, QString &plainText)
{
    //    qDebug() << "In updateBlockUsingPlainText";
    //    qDebug() << "plainText: " << plainText;
    unsigned int lineTotalIndentLength = calculateTotalIndentLength(plainText, blockInfo);
    blockInfo->setTotalIndentLength(lineTotalIndentLength);
    blockInfo->determineBlockType(plainText);
    if (blockInfo->blockType() == BlockInfo::BlockType::Divider)
        plainText = blockInfo->blockDelimiter();
    int lastPos = blockIndex > 0 ? m_blockList[blockIndex - 1]->lineEndPos() : -1;
    updateBlockText(blockInfo, plainText, lastPos + 1, lastPos + 1);
    blockInfo->setIndentLevel(0);
    if (blockInfo->parent() != nullptr) {
        blockInfo->parent()->removeChild(blockInfo);
        //        if (lineTotalIndentLength <= 0)
        //            blockInfo->setParent(nullptr);
    }
    //    blockInfo->setParent(nullptr);
    if (lineTotalIndentLength > 0 && m_blockList.length() > 0) {
        determineBlockIndentAndParentChildRelationship(blockInfo, blockIndex - 1);
    }
}

void BlockModel::loadText(const QString &text)
{
    QElapsedTimer timer;
    timer.start();

    emit aboutToLoadText();
    clear();
    m_sourceDocument.setPlainText(text); // 40% performance hit
    QStringList lines = text.split("\n"); // TODO: Should we use Qt::SkipEmptyParts?

    beginInsertRows(QModelIndex(), 0, lines.size() - 1);

    unsigned int blockIndex = 0;
    for (auto &line : lines) {
        QSharedPointer<BlockInfo> blockInfo = QSharedPointer<BlockInfo>(new BlockInfo());
        m_blockList << blockInfo;
        updateBlockUsingPlainText(blockInfo, blockIndex, line);
        blockIndex++;
    }

    endInsertRows();
    qDebug() << "Finished loading.";

    QJsonObject dataToSendToView{ { "itemIndexInView",
                                    m_itemIndexInView } };
    emit loadTextFinished(QVariant(dataToSendToView));

    qint64 elapsed = timer.elapsed();
    qDebug() << "Time taken:" << elapsed << "ms";
}

void BlockModel::clear()
{
    m_undoStack.clear();
    m_redoStack.clear();
    beginResetModel();
    //    qDeleteAll(m_blockList); // TODO: this is extremly slow 2-4x slower than loading. How to
    //    optimize this?
    m_blockList.clear();
    endResetModel();
}

void BlockModel::updateBlocksLinePositions(unsigned int blockPosition, int delta)
{
    for (unsigned int i = blockPosition; i < m_blockList.length(); i++) {
        QSharedPointer<BlockInfo> blockInfo = m_blockList[i];
//        qDebug() << "updated block plaintext: " << blockInfo->textPlainText();
//        qDebug() << "new lineStartPos: " << blockInfo->lineStartPos() + delta;
//        qDebug() << "new lineEndPos: " << blockInfo->lineEndPos() + delta;
        blockInfo->setLineStartPos(blockInfo->lineStartPos() + delta);
        blockInfo->setLineEndPos(blockInfo->lineEndPos() + delta);
    }
}

QString BlockModel::QmlHtmlToMarkdown(QString &qmlHtml)
{
    // Converting QML's TextArea text (HTML) to Markdown (Plaintext)
    //    qDebug() << "qmlHtml: " << qmlHtml;
    qmlHtml.replace("<br />", "\u2063"); // To preserve <br /> after toMarkdown() (otherwise it will
                                         // parse these as line breaks)
    QTextDocument doc;
    doc.setHtml(qmlHtml);
    QString markdown = doc.toMarkdown();
    //    qDebug() << "markdown 0: " << markdown;
    markdown.replace("\u2063", "<br />");
    markdown.replace("\n\n", ""); // due to toMarkdown() adding these
    markdown.replace("\n", " "); // due to QML's TextEdit text wrapping adding these
    //    qDebug() << "markdown 1: " << markdown;

    return markdown;
}

void BlockModel::setTextAtIndex(const int blockIndex, QString qmlHtml, int cursorPositionQML)
{
    emit aboutToChangeText();

    QSharedPointer<BlockInfo> blockInfo = m_blockList[blockIndex];

    QString markdown = QmlHtmlToMarkdown(qmlHtml);
    // Preserve delimiter
    markdown = blockInfo->blockDelimiter() + markdown;
    //    qDebug() << "markdown 2: " << markdown;
    // Preserve indentation
    if (blockInfo->totalIndentLength() > 0)
        markdown = blockInfo->indentedString() + markdown;

    if (blockInfo->textPlainText() == markdown) {
        emit textChangeFinished();
        return;
    }

    qDebug() << "cursorPositionQML: " << cursorPositionQML;
    //    qDebug() << "Changing!";
    //    qDebug() << "markdown 3: " << markdown;

//    int indentAndDelimiterLength =
//            blockInfo->indentedString().length() + blockInfo->blockDelimiter().length();
//    int cursorPosition = cursorPositionQML + indentAndDelimiterLength;
    if (abs(blockInfo->textPlainText().length() - markdown.length()) == 1) {
        // one char operation
        OneCharOperation oneCharOperation = blockInfo->textPlainText().length() > markdown.length()
                ? oneCharOperation = OneCharOperation::CharDelete
                : oneCharOperation = OneCharOperation::CharInsert;
        updateSourceTextBetweenLines(blockInfo->lineStartPos(), blockInfo->lineEndPos(), markdown,
                                     true, cursorPositionQML, ActionType::Modify, oneCharOperation);
    } else {
        // not one char operation
        updateSourceTextBetweenLines(blockInfo->lineStartPos(), blockInfo->lineEndPos(), markdown,
                                     true, cursorPositionQML, ActionType::Modify,
                                     OneCharOperation::NoOneCharOperation);
    }

    blockInfo->determineBlockType(markdown);
    int numberOfLinesBefore = blockInfo->lineEndPos() - blockInfo->lineStartPos() + 1;
    int numberOfLinesDelta = markdown.count('\n') + 1 - numberOfLinesBefore;
    updateBlockText(blockInfo, markdown, blockInfo->lineStartPos(),
                    blockInfo->lineEndPos() + numberOfLinesDelta);
    if (numberOfLinesDelta != 0) {
        updateBlocksLinePositions(blockIndex + 1, numberOfLinesDelta);
    }

    QModelIndex modelIdx = this->index(blockIndex);
    emit dataChanged(modelIdx, modelIdx, {}); // TODO: Empty vector means all roles, is that good?
    emit textChangeFinished();
}

// TODO: This function slows the app down (writing lag) when the text is very large (e.g. Moby Dick)
// And we don't really need to rely on QTextDocument for that
void BlockModel::updateSourceTextBetweenLines(int startLinePos, int endLinePos,
                                              const QString &newText, bool shouldCreateUndo,
                                              int cursorPosition, ActionType actionType,
                                              OneCharOperation oneCharoperation,
                                              bool isForceMergeLastAction,
                                              int firstBlockSelectionStart,
                                              int lastBlockSelectionEnd)
{
    //    qDebug() << "startLinePos: " << startLinePos;
    //    qDebug() << "endLinePos: " << endLinePos;
    qDebug() << "sourceDocument before: " << m_sourceDocument.toPlainText();

    SingleAction singleAction;
    bool isMergingLastAction = isForceMergeLastAction;

    if (shouldCreateUndo) {
        m_redoStack.clear(); // Clear redo stack after edit
        singleAction.actionType = actionType;
        singleAction.blockStartIndex = startLinePos;
        singleAction.blockEndIndex = endLinePos;
        singleAction.newPlainText = newText;
        singleAction.oneCharOperation = oneCharoperation;
        singleAction.lastCursorPosition = cursorPosition;

        if (actionType == ActionType::Modify) {
            // Modifying only one block
            qDebug() << "In modify";
            if (!m_undoStack.isEmpty() && !m_undoStack.last().actions.isEmpty()) {
                SingleAction &lastAction = m_undoStack.last().actions.last();
                if (oneCharoperation == OneCharOperation::CharInsert
                    || oneCharoperation == OneCharOperation::CharDelete) {
                    if (lastAction.oneCharOperation == oneCharoperation
                        && lastAction.blockStartIndex == startLinePos
                        && lastAction.blockEndIndex == endLinePos
                        && abs(lastAction.lastCursorPosition - cursorPosition) == 1) {

                        qDebug() << "Merging last char action";
                        isMergingLastAction = true;
                        lastAction.lastCursorPosition = cursorPosition;
                        lastAction.newPlainText = newText;
                    }
                }
            }
        }

        if (actionType == ActionType::Remove) {
            singleAction.firstBlockSelectionStart = firstBlockSelectionStart;
            singleAction.lastBlockSelectionEnd = lastBlockSelectionEnd;
        }
    }

    QTextBlock startBlock = m_sourceDocument.findBlockByLineNumber(startLinePos);
    QTextBlock endBlock = m_sourceDocument.findBlockByLineNumber(endLinePos);
    QTextCursor cursor(startBlock);

    // position cursor at end of endBlock while keeping the anchoe
    qDebug() << "startLinePos: " << startLinePos;
    qDebug() << "endLinePos:" << endLinePos;
    qDebug() << "endBlock.position(): " << endBlock.position();
    qDebug() << "endBlock.length(): " << endBlock.length();
    cursor.setPosition(endBlock.position() + endBlock.length() - 1, QTextCursor::KeepAnchor);
    qDebug() << "cursor.selectedText(): " << cursor.selectedText();

    singleAction.oldPlainText = cursor.selectedText().replace(
            "\u2029", "\n"); // In selectedText(), line breaks '\n' are replaced with Unicode
                             // U+2029. See: https://doc.qt.io/qt-6/qtextcursor.html#selectedText

    if (actionType == ActionType::Modify) {
        cursor.removeSelectedText();
        cursor.insertText(newText);
    } else if (actionType == ActionType::Remove) {
        qDebug() << "remove startLinePos: " << startLinePos;
        int positionToRemoveFrom = startBlock.position()
                - (startBlock.position() > 0 && startLinePos > 0
                           ? 1
                           : 0); // we want to remove the newline character at the start as well, if
                                 // exist
        cursor.setPosition(positionToRemoveFrom, QTextCursor::MoveAnchor);
        cursor.setPosition(endBlock.position() + endBlock.length() - 1, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
    } else if (actionType == ActionType::Insert) {
        qDebug() << "startLinePos: " << startLinePos;
        qDebug() << "startBlock.position(): " << startBlock.position();
        qDebug() << "characterCount: " << m_sourceDocument.characterCount();
        if (startLinePos > m_sourceDocument.lineCount() - 1) {
            qDebug() << "inserting at the end of the document";
            cursor.setPosition(m_sourceDocument.characterCount() - 1, QTextCursor::MoveAnchor);
        } else {
            qDebug() << "inserting at the middle of the document";
            cursor.setPosition(startBlock.position(), QTextCursor::MoveAnchor);
        }
        if (startLinePos > m_sourceDocument.lineCount() - 1) {
            // If we're inserting at the end of the document
            qDebug() << "inserting newline";
            cursor.insertText("\n" + newText);
        } else {
            // If we're inserting at the middle of the document
            qDebug() << "not inserting newline";
            cursor.insertText(newText + "\n");
        }
    }

    if (isForceMergeLastAction && !m_undoStack.isEmpty()) {
        m_undoStack.last().actions.push_back(singleAction);
    }

    if (!isMergingLastAction && shouldCreateUndo) {
        qDebug() << "new CompoundAction";
        CompoundAction compoundAction;
        compoundAction.actions.push_back(singleAction);
        m_undoStack.push_back(compoundAction);
        qDebug() << "m_undoStack in KB: " << estimateMemoryUsageInKB(m_undoStack);
    }

    qDebug() << "sourceDocument after: " << m_sourceDocument.toPlainText();
}

double BlockModel::estimateMemoryUsageInKB(const QList<CompoundAction> &undoStack)
{
    int totalSize = 0;
    // Size of QList overhead
    totalSize += sizeof(QList<CompoundAction>);
    for (const CompoundAction &compoundAction : undoStack) {
        // Size of QList<SingleAction> overhead
        totalSize += sizeof(QList<SingleAction>);
        for (const SingleAction &action : compoundAction.actions) {
            // Size of two unsigned int and one enum (assuming it's sizeof(int))
            totalSize += 2 * sizeof(unsigned int) + sizeof(int);
            // Size of QString overhead and its content
            totalSize += 2 * sizeof(QString) + action.oldPlainText.size() * sizeof(QChar)
                    + action.newPlainText.size() * sizeof(QChar);
            // Size of OneCharOperation
            totalSize += sizeof(OneCharOperation);
            // Size of int
            totalSize += sizeof(int);
        }
    }

    // Convert totalSize (in bytes) to kilobytes
    return static_cast<double>(totalSize) / 1024.0;
}

void BlockModel::indentBlocks(QList<int> selectedBlockIndexes)
{
    std::sort(selectedBlockIndexes.begin(), selectedBlockIndexes.end());

    if (selectedBlockIndexes.length() > 0) {
        // Running on the selected blocks to check for indentation
        int numberOfAlreadyIndentedBlocks = 0;
        for (int i = selectedBlockIndexes[0];
             i < selectedBlockIndexes[selectedBlockIndexes.length() - 1] + 1; i++) {
            QSharedPointer<BlockInfo> blockInfo = m_blockList[i];
            if (blockInfo->isIndentable() && i > 0) {
                emit aboutToChangeText();

                // Traverse the list backwards from current block position to check for a matching
                // parent. If none found (or an empty block encountered), nothing is done.
                for (int j = i - 1; j >= 0; j--) {
                    QSharedPointer<BlockInfo> previousBlock = m_blockList[j];

                    if (previousBlock->indentLevel() < blockInfo->indentLevel()) {
                        break;
                    }

                    if (previousBlock->indentLevel() == blockInfo->indentLevel()) {

                        QString newIndentedString = previousBlock->indentedString() + '\t';
                        QString newIndentedPlainText = blockInfo->textPlainText();
                        newIndentedPlainText = newIndentedPlainText.sliced(
                                blockInfo->indentedString().length(),
                                newIndentedPlainText.length()
                                        - blockInfo->indentedString().length());
                        newIndentedPlainText = newIndentedString + newIndentedPlainText;

                        blockInfo->setIndentedString(newIndentedString);
                        blockInfo->setTotalIndentLength(previousBlock->totalIndentLength()
                                                        + m_tabLengthInSpaces);
                        blockInfo->setIndentLevel(blockInfo->indentLevel() + 1);

                        updateSourceTextBetweenLines(
                                blockInfo->lineStartPos(), blockInfo->lineEndPos(),
                                newIndentedPlainText, true, 0, ActionType::Modify,
                                OneCharOperation::Indent,
                                numberOfAlreadyIndentedBlocks <= 0 ? false : true);
                        numberOfAlreadyIndentedBlocks++; // We want only the first indented block to
                                                         // create an undo compund action, the rest
                                                         // will be merged with it

                        updateBlockText(blockInfo, newIndentedPlainText, blockInfo->lineStartPos(),
                                        blockInfo->lineEndPos());

                        if (blockInfo->parent() != nullptr) {
                            blockInfo->parent()->removeChild(blockInfo);
                        }
                        blockInfo->setParent(previousBlock);
                        previousBlock->addChild(blockInfo);

                        // We need to run on all the children and ask them to reavaluate their
                        // parent
                        for (auto &child : blockInfo->children()) {
                            determineBlockIndentAndParentChildRelationship(
                                    child, m_blockList.indexOf(child) - 1);
                        }

                        //                        previousBlock->addChild(blockInfo);

                        QModelIndex modelIdx = this->index(i);
                        emit dataChanged(modelIdx, modelIdx, {});

                        break;
                    }
                }

                emit textChangeFinished();
            }
        }
    }
}

void BlockModel::unindentBlock(unsigned int blockIndex, QSharedPointer<BlockInfo> &blockInfo,
                               bool isSecondRun, int numberOfAlreadyUnindentedBlocks)
{
    qDebug() << "unindentBlock!";
    emit aboutToChangeText();
    QString plainTextWithoutIndentation =
            blockInfo->textPlainText().mid(blockInfo->indentedString().length());
    //    qDebug() << "is parent null: " << (blockInfo->parent() == nullptr);
    //    qDebug() << "parent text: " << blockInfo->parent()->textPlainText();
    //    qDebug() << "parent indentedString: " << blockInfo->parent()->indentedString();
    QString newPlainText = blockInfo->parent()->indentedString() + (isSecondRun ? "\t" : "")
            + plainTextWithoutIndentation;

    blockInfo->setIndentedString(blockInfo->parent()->indentedString() + (isSecondRun ? "\t" : ""));
    blockInfo->setTotalIndentLength(blockInfo->parent()->totalIndentLength()
                                    + (isSecondRun ? m_tabLengthInSpaces : 0));
    blockInfo->setIndentLevel(blockInfo->parent()->indentLevel() + (isSecondRun ? 1 : 0));

    determineBlockIndentAndParentChildRelationship(blockInfo, blockIndex - 1);

    updateSourceTextBetweenLines(blockInfo->lineStartPos(), blockInfo->lineEndPos(), newPlainText,
                                 true, 0, ActionType::Modify, OneCharOperation::Unindent,
                                 numberOfAlreadyUnindentedBlocks <= 0 ? false : true);

    updateBlockText(blockInfo, newPlainText, blockInfo->lineStartPos(), blockInfo->lineEndPos());

    QModelIndex modelIdx = this->index(blockIndex);
    emit dataChanged(modelIdx, modelIdx, {});
    emit textChangeFinished();
}

void BlockModel::unindentBlocks(QList<int> selectedBlockIndexes)
{
    qDebug() << "Unindent: " << selectedBlockIndexes;
    QList<QSharedPointer<BlockInfo>> unindentedAlready{};
    std::sort(selectedBlockIndexes.begin(), selectedBlockIndexes.end());

    if (selectedBlockIndexes.length() > 0) {
        for (int i = selectedBlockIndexes[0];
             i < selectedBlockIndexes[selectedBlockIndexes.length() - 1] + 1; i++) {
            QSharedPointer<BlockInfo> blockInfo = m_blockList[i];
            if (blockInfo->isIndentable() && i > 0) {
                qDebug() << "1";
                qDebug() << blockInfo->indentLevel();
                qDebug() << bool(blockInfo->parent() == nullptr);
                unsigned int originalBlockIndentLevel = blockInfo->indentLevel();
                if (blockInfo->indentLevel() > 0 && blockInfo->parent() != nullptr
                    && !unindentedAlready.contains(blockInfo)) {
                    qDebug() << "2";
                    unindentBlock(i, blockInfo, false, unindentedAlready.length());
                    unindentedAlready.push_back(blockInfo);
                    int j = i + 1;
                    while (j < m_blockList.length()) {
                        qDebug() << "3";
                        QSharedPointer<BlockInfo> nextBlock = m_blockList[j];

                        if (nextBlock->indentLevel() <= originalBlockIndentLevel) {
                            break;
                        }

                        qDebug() << "4";

                        if (nextBlock->indentLevel() > 0 && nextBlock->parent() != nullptr
                            && nextBlock->indentLevel() > originalBlockIndentLevel
                            && !unindentedAlready.contains(nextBlock)) {
                            qDebug() << "4.5";
                            unindentBlock(j, nextBlock, true, unindentedAlready.length());
                            unindentedAlready.push_back(nextBlock);
                        }

                        qDebug() << "5";

                        j++;
                    }
                }
            }
        }
    }
}

void BlockModel::moveBlockTextToBlockAbove(int blockIndex)
{
    if (blockIndex > 0) {
        QSharedPointer<BlockInfo> blockInfo = m_blockList[blockIndex];
        QSharedPointer<BlockInfo> previousBlock = m_blockList[blockIndex - 1];

        emit aboutToChangeText();

        updateSourceTextBetweenLines(previousBlock->lineStartPos(), previousBlock->lineEndPos(),
                                     previousBlock->textPlainText() + blockInfo->textPlainText());

        updateBlockText(previousBlock, previousBlock->textPlainText() + blockInfo->textPlainText(),
                        previousBlock->lineStartPos(), previousBlock->lineEndPos());

        QModelIndex modelIdx = this->index(blockIndex - 1);
        emit dataChanged(modelIdx, modelIdx, {});

        updateBlocksLinePositions(blockIndex + 1, -1); // decrease all blocks lines data by 1

        emit textChangeFinished();
    }
}

void BlockModel::backSpacePressedAtStartOfBlock(int blockIndex)
{
    QSharedPointer<BlockInfo> blockInfo = m_blockList[blockIndex];

    if (blockInfo->blockType() != BlockInfo::BlockType::RegularText
        && !blockInfo->blockDelimiter().isEmpty()) {
        // if this block has a delimiter -> we remove it
        emit aboutToChangeText();

        QString blockPlainText = blockInfo->textPlainText();
        QString afterIndentRemoval = blockPlainText.mid(blockInfo->indentedString().length());
        QString newBlockPlainText = afterIndentRemoval.mid(blockInfo->blockDelimiter().length());
        newBlockPlainText = blockInfo->indentedString() + newBlockPlainText;

        //        updateSourceTextBetweenLines(blockInfo->lineStartPos(),
        //                                     blockInfo->lineEndPos(),
        //                                     newBlockPlainText);

        updateSourceTextBetweenLines(blockInfo->lineStartPos(), blockInfo->lineEndPos(),
                                     newBlockPlainText, true, 0, ActionType::Modify,
                                     OneCharOperation::NoOneCharOperation, false);

        blockInfo->determineBlockType(newBlockPlainText);

        updateBlockText(blockInfo, newBlockPlainText, blockInfo->lineStartPos(),
                        blockInfo->lineEndPos());

        QModelIndex modelIdx = this->index(blockIndex);
        emit dataChanged(modelIdx, modelIdx, {});
        emit textChangeFinished();
    } else if (blockInfo->indentLevel() > 0) {
        // if this block is a regularText but indented -> we unindent it
        unindentBlocks({ blockIndex });
    } else if (blockIndex > 0) {
        // if this block is a regularText and not indented -> we remove it
        emit aboutToChangeText();

        updateSourceTextBetweenLines(blockInfo->lineStartPos(), blockInfo->lineEndPos(), "", true,
                                     0, ActionType::Remove, OneCharOperation::NoOneCharOperation,
                                     true);
        emit textChangeFinished();
        beginRemoveRows(QModelIndex(), blockIndex, blockIndex);
        // We need to run on all the children and ask them to reavaluate their parent
        m_blockList.removeAt(blockIndex);
        if (blockInfo->parent() != nullptr) {
            blockInfo->parent()->removeChild(blockInfo);
            //            blockInfo->setParent(nullptr);
        }
        for (auto &child : blockInfo->children()) {
            if (child->parent() != nullptr)
                child->parent()->removeChild(child);
            //            child->setParent(nullptr);
            determineBlockIndentAndParentChildRelationship(child, blockIndex - 1);
        }
        //        blockInfo->deleteLater();
        endRemoveRows();
    }
}

void BlockModel::insertNewBlock(int blockIndex, QString qmlHtml, bool shouldMergeWithPreviousAction)
{
    qDebug() << "In insertNewBlock";
    emit aboutToChangeText();
    beginInsertRows(QModelIndex(), blockIndex + 1, blockIndex + 1);
    QSharedPointer<BlockInfo> previousBlockInfo = m_blockList[blockIndex];
    //    BlockInfo *newBlockInfo = new BlockInfo(this);
    QSharedPointer<BlockInfo> newBlockInfo = QSharedPointer<BlockInfo>(new BlockInfo());

    newBlockInfo->setTotalIndentLength(previousBlockInfo->totalIndentLength());
    newBlockInfo->setIndentedString(previousBlockInfo->indentedString());
    newBlockInfo->determineBlockType(
            previousBlockInfo->textPlainText()); // TODO: set block type and delimteter without this

    QString markdown = qmlHtml.isEmpty() ? QStringLiteral("") : QmlHtmlToMarkdown(qmlHtml);
    // If the previous block is an item in a numbered list we increase the delimiter's number by 1
    if (newBlockInfo->blockType() == BlockInfo::BlockType::NumberedListItem) {
        int indexOfDotInDelimiter = newBlockInfo->blockDelimiter().indexOf(".");
        if (indexOfDotInDelimiter != -1) {
            int previousBlockNumber =
                    newBlockInfo->blockDelimiter().sliced(0, indexOfDotInDelimiter).toInt();
            newBlockInfo->setBlockDelimiter(QString::number(previousBlockNumber + 1) + ". ");
        }
    }

    // If the previous block is a checked task item, we need to make this an uncheked task
    if (previousBlockInfo->blockType() == BlockInfo::BlockType::Todo
        && previousBlockInfo->metaData()["taskChecked"].toBool()) {
        QString newTaskDelimiter = previousBlockInfo->blockDelimiter();
        newTaskDelimiter.replace("x", " ");
        newTaskDelimiter.replace("X", " ");
        newBlockInfo->setBlockDelimiter(newTaskDelimiter);
        newBlockInfo->updateMetaData("taskChecked", false);
    }

    // If the previous block is a divider or a quote and an enter is pressed, we simply create a
    // regular empty block
    if (previousBlockInfo->blockType() == BlockInfo::BlockType::Divider
        || previousBlockInfo->blockType() == BlockInfo::BlockType::Quote
        || previousBlockInfo->blockType() == BlockInfo::BlockType::DropCap) {
        newBlockInfo->setBlockDelimiter("");
        newBlockInfo->setBlockType(BlockInfo::BlockType::RegularText);
        newBlockInfo->setTextPlainText("");
        markdown = "";
    }

    // Preserve delimiter
    markdown = newBlockInfo->blockDelimiter() + markdown;
    // Preserve indentation
    //    qDebug() << "newBlockInfo->totalindentlength: " << newBlockInfo->totalIndentLength();
    if (newBlockInfo->totalIndentLength() > 0)
        markdown = newBlockInfo->indentedString() + markdown;

    qDebug() << "previousBlockInfo->lineEndPos() + 1: " << previousBlockInfo->lineEndPos() + 1;
    qDebug() << "shouldMergeWithPreviousAction: " << shouldMergeWithPreviousAction;

    updateSourceTextBetweenLines(previousBlockInfo->lineEndPos() + 1,
                                 previousBlockInfo->lineEndPos() + 1, markdown, true, 0,
                                 ActionType::Insert, OneCharOperation::NoOneCharOperation,
                                 shouldMergeWithPreviousAction);

    updateBlockText(newBlockInfo, markdown, previousBlockInfo->lineEndPos() + 1,
                    previousBlockInfo->lineEndPos() + 1);

    qDebug() << "newBlockInfo->lineStartPos(): " << newBlockInfo->lineStartPos();
    qDebug() << "newBlockInfo->lineEndPos(): " << newBlockInfo->lineEndPos();

    newBlockInfo->setIndentLevel(previousBlockInfo->indentLevel());
    //    newBlockInfo->setParent(nullptr);
    if (newBlockInfo->totalIndentLength() > 0 && m_blockList.length() > 0) {
        qDebug() << "DDDDDDDDDDDDD";
        determineBlockIndentAndParentChildRelationship(newBlockInfo, blockIndex);
    }
    emit newBlockCreated(blockIndex + 1);
    m_blockList.insert(blockIndex + 1, newBlockInfo);
    endInsertRows();
    emit textChangeFinished();

    updateBlocksLinePositions(blockIndex + 2, 1);
}

BlockInfo::BlockType BlockModel::getBlockType(int blockIndex)
{
    if (blockIndex < 0 || blockIndex > m_blockList.length() - 1)
        return BlockInfo::BlockType::RegularText;

    QSharedPointer<BlockInfo> blockInfo = m_blockList[blockIndex];
    return blockInfo->blockType();
}

void BlockModel::toggleTaskAtIndex(int blockIndex)
{
    emit aboutToChangeText();
    QSharedPointer<BlockInfo> blockInfo = m_blockList[blockIndex];
    if (blockInfo->blockType() != BlockInfo::BlockType::Todo)
        return;

    QString newDelimiter = blockInfo->blockDelimiter();
    if (blockInfo->blockDelimiter().contains('x')) {
        newDelimiter.replace("x", " ");
        blockInfo->updateMetaData("taskChecked", false);
        qDebug() << "taskChecked: " << false;
    } else if (blockInfo->blockDelimiter().contains('X')) {
        newDelimiter.replace("X", " ");
        blockInfo->updateMetaData("taskChecked", false);
        qDebug() << "taskChecked: " << false;
    } else {
        newDelimiter.replace("[ ]", "[x]");
        blockInfo->updateMetaData("taskChecked", true);
        qDebug() << "taskChecked: " << true;
    }
    blockInfo->setBlockDelimiter(newDelimiter);

    QString plainText = blockInfo->textPlainText().mid(blockInfo->indentedString().length()
                                                       + blockInfo->blockDelimiter().length());
    // New delimiter
    plainText = blockInfo->blockDelimiter() + plainText;
    // Preserve indentation
    if (blockInfo->totalIndentLength() > 0)
        plainText = blockInfo->indentedString() + plainText;

    updateBlockText(blockInfo, plainText, blockInfo->lineStartPos(), blockInfo->lineEndPos());

    updateSourceTextBetweenLines(blockInfo->lineStartPos(), blockInfo->lineEndPos(), plainText,
                                 true, 0, ActionType::Modify, OneCharOperation::NoOneCharOperation,
                                 false);

    QModelIndex modelIdx = this->index(blockIndex);
    emit dataChanged(modelIdx, modelIdx, {});
    emit textChangeFinished();
}

void BlockModel::editBlocks(QList<int> selectedBlockIndexes, int firstBlockSelectionStart,
                            int lastBlockSelectionEnd, int savedPressedChar,
                            bool isPressedCharLower)
{
    if (selectedBlockIndexes.length() < 2)
        return;

    emit aboutToChangeText();
    std::sort(selectedBlockIndexes.begin(), selectedBlockIndexes.end());

    qDebug() << "firstBlockSelectionStart: " << firstBlockSelectionStart;
    qDebug() << "lastBlockSelectionEnd: " << lastBlockSelectionEnd;

    QSharedPointer<BlockInfo> firstBlock = m_blockList[selectedBlockIndexes[0]];
    int lastBlockIndex = selectedBlockIndexes[selectedBlockIndexes.length() - 1];
    QSharedPointer<BlockInfo> lastBlock = m_blockList[lastBlockIndex];
    QString savedTextFirstBlock = firstBlock->textPlainText().mid(
            firstBlock->indentedString().length() + firstBlock->blockDelimiter().length());
    qDebug() << "savedTextFirstBlock 1: " << savedTextFirstBlock;
    savedTextFirstBlock = firstBlock->indentedString() + firstBlock->blockDelimiter()
            + savedTextFirstBlock.mid(0, firstBlockSelectionStart);
    qDebug() << "savedTextFirstBlock 2: " << savedTextFirstBlock;
    qDebug() << "lastBlock->textPlainText: " << lastBlock->textPlainText();
    qDebug() << "lastBlock->textPlainText length: " << lastBlock->textPlainText().length();
    int lineBreakCount = lastBlock->textPlainText().count("<br />");
    int lineBreaksLength =
            (lineBreakCount * QStringLiteral("<br />").length()) - lineBreakCount;
    qDebug() << "lastBlock->indentedString().length(): " << lastBlock->indentedString().length();
    qDebug() << "lastBlock->blockDelimiter().length(): " << lastBlock->blockDelimiter().length();
    qDebug() << "lastBlockSelectionEnd: " << lastBlockSelectionEnd;
    qDebug() << "lineBreaksLength: " << lineBreaksLength;
    QString savedTextLastBlock = lastBlock->textPlainText().mid(
            lastBlock->indentedString().length() + lastBlock->blockDelimiter().length()
            + lastBlockSelectionEnd + lineBreaksLength);
    qDebug() << "savedTextLastBlock: " << savedTextLastBlock;

    QString savedPressedCharString = QKeySequence(savedPressedChar).toString();
    savedPressedCharString =
            isPressedCharLower ? savedPressedCharString.toLower() : savedPressedCharString;

    QString newFirstBlockPlainText =
            savedTextFirstBlock + savedPressedCharString + savedTextLastBlock;

    qDebug() << "newFirstBlockPlainText: " << newFirstBlockPlainText;
    updateBlockText(firstBlock, newFirstBlockPlainText, firstBlock->lineStartPos(),
                    firstBlock->lineEndPos());
    QModelIndex firstBlockIndex = this->index(selectedBlockIndexes[0]);
    emit dataChanged(firstBlockIndex, firstBlockIndex, {});

    updateSourceTextBetweenLines(
            selectedBlockIndexes[1], selectedBlockIndexes[selectedBlockIndexes.length() - 1], "",
            true, 0, ActionType::Remove, OneCharOperation::NoOneCharOperation, false,
            firstBlockSelectionStart, lastBlockSelectionEnd);

    updateSourceTextBetweenLines(firstBlock->lineStartPos(), firstBlock->lineEndPos(),
                                 newFirstBlockPlainText, true, 0, ActionType::Modify,
                                 OneCharOperation::NoOneCharOperation, true);

    // Find the lowest indent level in selected blocks
    QList<int> allIndentLevels = {};
    for (int i = lastBlockIndex; i < m_blockList.length(); i++) {
        allIndentLevels.push_back(m_blockList[i]->indentLevel());
    }
    unsigned int minIndentLevel = *std::min_element(allIndentLevels.begin(), allIndentLevels.end());

    beginRemoveRows(QModelIndex(), selectedBlockIndexes[1],
                    selectedBlockIndexes[selectedBlockIndexes.length() - 1]);
    qDebug() << "start remove: " << selectedBlockIndexes[1];
    qDebug() << "end remove: " << selectedBlockIndexes[selectedBlockIndexes.length() - 1];
    for (int i = selectedBlockIndexes[1];
         i <= selectedBlockIndexes[selectedBlockIndexes.length() - 1]; i++) {
        QSharedPointer<BlockInfo> blockToRemove = m_blockList[i];
        for (auto &child : blockToRemove->children()) {
            if (child->parent() != nullptr)
                child->parent()->removeChild(child);
            //            child->setParent(nullptr);
        }
        if (blockToRemove->parent() != nullptr)
            blockToRemove->parent()->removeChild(blockToRemove);
        //        blockToRemove->deleteLater();
    }
    m_blockList.remove(selectedBlockIndexes[1], selectedBlockIndexes.length() - 1);
    endRemoveRows();

    // Redetermine child parent relationship for all blocks with indent level higher then the lowest
    // indent level found
    for (int i = selectedBlockIndexes[0] + 1; i < m_blockList.length(); i++) {
        QSharedPointer<BlockInfo> blockInfo = m_blockList[i];

        // We stop if we reach a block with an indent level equal/smaller than minIndentLevel
        if (blockInfo->indentLevel() <= minIndentLevel)
            break;

        if (blockInfo->indentLevel() > minIndentLevel) {
            determineBlockIndentAndParentChildRelationship(blockInfo, i - 1);
            QModelIndex modelIdxOfBlockIndentChanged = this->index(i);
            emit dataChanged(modelIdxOfBlockIndentChanged, modelIdxOfBlockIndentChanged, {});
        }
    }

    qDebug() << "selectedBlockIndexes: " << selectedBlockIndexes;
    qDebug() << "delta: " << -(selectedBlockIndexes.length() - 1);
    updateBlocksLinePositions(selectedBlockIndexes[1], -(selectedBlockIndexes.length() - 1));

    emit textChangeFinished();
}

void BlockModel::undo()
{
    if (!m_undoStack.isEmpty()) {
        emit aboutToChangeText();
        qDebug() << "m_undoStack.length: " << m_undoStack.length();
        CompoundAction &lastCompoundAction = m_undoStack.last();
        m_redoStack.push_back(lastCompoundAction);
        SingleAction actionWithSelectionToRestore;
        bool shouldRestoreSelection = false;
        int indexSingle = 0;
        for (auto &singleAction : lastCompoundAction.actions) {
            qDebug() << "indexSingle: " << indexSingle;
            indexSingle++;
            if (singleAction.actionType == ActionType::Modify) {
                qDebug() << "In undo Modify";
                unsigned int modifiedBlockIndex = singleAction.blockStartIndex;
                QSharedPointer<BlockInfo> blockInfo = m_blockList[modifiedBlockIndex];
                updateBlockUsingPlainText(blockInfo, blockInfo->lineStartPos(),
                                          singleAction.oldPlainText);
                updateSourceTextBetweenLines(singleAction.blockStartIndex,
                                             singleAction.blockEndIndex, singleAction.oldPlainText,
                                             false);
                emit restoreCursorPosition(singleAction.lastCursorPosition);
                qDebug() << "Here 1";
                QModelIndex modelIdx = this->index(modifiedBlockIndex);
                qDebug() << "Here 2";
                emit dataChanged(modelIdx, modelIdx, {});
                qDebug() << "Here 3";
            } else if (singleAction.actionType == ActionType::Remove) {
                qDebug() << "In undo Remove";
                qDebug() << "blockStartIndex: " << singleAction.blockStartIndex;
                qDebug() << "blockEndIndex: " << singleAction.blockEndIndex;
                qDebug() << "oldPlainText: " << singleAction.oldPlainText;
                updateSourceTextBetweenLines(singleAction.blockStartIndex,
                                             singleAction.blockEndIndex, singleAction.oldPlainText,
                                             false, 0, ActionType::Insert,
                                             OneCharOperation::NoOneCharOperation, false);
                QStringList lines =
                        singleAction.oldPlainText.split("\n"); // TODO: consider optimizing this
                qDebug() << "lines: " << lines;
                beginInsertRows(QModelIndex(), singleAction.blockStartIndex,
                                singleAction.blockEndIndex);
                int index = 0;
                for (auto &line : lines) {
                    int blockIndex = singleAction.blockStartIndex + index;
//                    qDebug() << "blockIndex: " << blockIndex;
                    //                    BlockInfo *blockInfo = new BlockInfo(this);
                    QSharedPointer<BlockInfo> blockInfo =
                            QSharedPointer<BlockInfo>(new BlockInfo());
                    m_blockList.insert(blockIndex, blockInfo);
                    updateBlockUsingPlainText(blockInfo, blockIndex, line);
//                    qDebug() << "blockStartLinePos:" << blockInfo->lineStartPos();
//                    qDebug() << "blockEndLinePos:" << blockInfo->lineEndPos();
                    index++;
                }
                endInsertRows();
                updateBlocksLinePositions(singleAction.blockEndIndex + 1, lines.length());

                actionWithSelectionToRestore.blockStartIndex = singleAction.blockStartIndex;
                actionWithSelectionToRestore.blockEndIndex = singleAction.blockEndIndex;
                actionWithSelectionToRestore.firstBlockSelectionStart =
                        singleAction.firstBlockSelectionStart;
                actionWithSelectionToRestore.lastBlockSelectionEnd =
                        singleAction.lastBlockSelectionEnd;
                shouldRestoreSelection = true;
                QSharedPointer<BlockInfo> firstBlockInfo = m_blockList[singleAction.blockStartIndex - 1];
                for (int i = singleAction.blockEndIndex + 1; i < m_blockList.length(); i++) {
                    QSharedPointer<BlockInfo> blockInfo = m_blockList[i];
                    if (blockInfo->indentedString().length() == 0 ||
                        blockInfo->indentedString().length() <= firstBlockInfo->indentedString().length()) {
                        break;
                    }
                    determineBlockIndentAndParentChildRelationship(blockInfo, i - 1);
                    QModelIndex modelIdx = this->index(i);
                    emit dataChanged(modelIdx, modelIdx, {});
                }
            } else if (singleAction.actionType == ActionType::Insert) {
                qDebug() << "In undo Insert";
                int blockIndex = singleAction.blockStartIndex;
                QSharedPointer<BlockInfo> blockInfo = m_blockList[blockIndex];
                beginRemoveRows(QModelIndex(), blockIndex, blockIndex);
                updateSourceTextBetweenLines(blockInfo->lineStartPos(), blockInfo->lineEndPos(), "",
                                             false, 0, ActionType::Remove,
                                             OneCharOperation::NoOneCharOperation, false);
                for (auto &child : blockInfo->children()) {
                    if (child->parent() != nullptr)
                        child->parent()->removeChild(child);
                    //                    child->setParent(nullptr);
                    determineBlockIndentAndParentChildRelationship(child, blockIndex - 1);
                }
                m_blockList.removeAt(blockIndex);
                //                blockInfo->deleteLater();
                endRemoveRows();
                updateBlocksLinePositions(blockIndex, -1);
                emit blockToFocusOnChanged(blockIndex - 1);
            }
        }


        qDebug() << "Here 4";
        m_undoStack.removeLast();
        qDebug() << "Here 5";
        emit textChangeFinished();
        if (shouldRestoreSelection) {
            emit restoreSelection(actionWithSelectionToRestore.blockStartIndex, actionWithSelectionToRestore.blockEndIndex, actionWithSelectionToRestore.firstBlockSelectionStart, actionWithSelectionToRestore.lastBlockSelectionEnd);
        }
    }
}

void BlockModel::redo()
{
    if (!m_redoStack.isEmpty()) {
        emit aboutToChangeText();
        qDebug() << "m_redoStack.length: " << m_redoStack.length();
        CompoundAction &lastCompoundAction = m_redoStack.last();
        m_undoStack.push_back(lastCompoundAction);

        for (auto &singleAction : lastCompoundAction.actions) {
            if (singleAction.actionType == ActionType::Modify) {
                qDebug() << "In redo Modify";
                unsigned int modifiedBlockIndex = singleAction.blockStartIndex;
                QSharedPointer<BlockInfo> blockInfo = m_blockList[modifiedBlockIndex];
                updateBlockUsingPlainText(blockInfo, blockInfo->lineStartPos(),
                                          singleAction.newPlainText);
                updateSourceTextBetweenLines(singleAction.blockStartIndex,
                                             singleAction.blockEndIndex, singleAction.newPlainText,
                                             false);
                emit restoreCursorPosition(singleAction.lastCursorPosition);
                QModelIndex modelIdx = this->index(modifiedBlockIndex);
                emit dataChanged(modelIdx, modelIdx, {});
            } else if (singleAction.actionType == ActionType::Remove) {
                qDebug() << "In redo Remove";
                updateSourceTextBetweenLines(
                        singleAction.blockStartIndex, singleAction.blockEndIndex, "", false, 0,
                        ActionType::Remove, OneCharOperation::NoOneCharOperation, false);
                for (unsigned int blockIndex = singleAction.blockStartIndex;
                     blockIndex <= singleAction.blockEndIndex; blockIndex++) {
                    QSharedPointer<BlockInfo> blockInfo = m_blockList[blockIndex];
                    for (auto &child : blockInfo->children()) {
                        if (child->parent() != nullptr)
                            child->parent()->removeChild(child);
                        //                        child->setParent(nullptr);
                        determineBlockIndentAndParentChildRelationship(child, blockIndex - 1);
                    }
                    //                    blockInfo->deleteLater();
                }
                beginRemoveRows(QModelIndex(), singleAction.blockStartIndex,
                                singleAction.blockEndIndex);
                m_blockList.remove(singleAction.blockStartIndex,
                                   singleAction.blockEndIndex - singleAction.blockStartIndex + 1);
                endRemoveRows();
                emit blockToFocusOnChanged(singleAction.blockStartIndex - 1);
                updateBlocksLinePositions(
                        singleAction.blockStartIndex,
                        -(singleAction.blockEndIndex - singleAction.blockStartIndex + 1));
                QSharedPointer<BlockInfo> firstBlockInfo = m_blockList[singleAction.blockStartIndex - 1];
                for (int i = singleAction.blockStartIndex; i < m_blockList.length(); i++) {
                    QSharedPointer<BlockInfo> blockInfo = m_blockList[i];
                    if (blockInfo->indentedString().length() == 0 ||
                        blockInfo->indentedString().length() <= firstBlockInfo->indentedString().length()) {
                        break;
                    }
                    determineBlockIndentAndParentChildRelationship(blockInfo, i - 1);
                    QModelIndex modelIdx = this->index(i);
                    emit dataChanged(modelIdx, modelIdx, {});
                }
            } else if (singleAction.actionType == ActionType::Insert) {
                qDebug() << "In redo Insert";
                int blockIndex = singleAction.blockStartIndex;
                updateSourceTextBetweenLines(singleAction.blockStartIndex,
                                             singleAction.blockEndIndex, singleAction.newPlainText,
                                             false, 0, ActionType::Insert,
                                             OneCharOperation::NoOneCharOperation, false);
                beginInsertRows(QModelIndex(), blockIndex, blockIndex);
                //                BlockInfo *blockInfo = new BlockInfo(this);
                QSharedPointer<BlockInfo> blockInfo = QSharedPointer<BlockInfo>(new BlockInfo());
                m_blockList.insert(blockIndex, blockInfo);
                updateBlockUsingPlainText(blockInfo, blockIndex, singleAction.newPlainText);
                endInsertRows();
                emit blockToFocusOnChanged(blockIndex);
                updateBlocksLinePositions(blockIndex + 1, 1);
            }
        }

        m_redoStack.removeLast();
        emit textChangeFinished();
    }
}

void BlockModel::setVerticalScrollBarPosition(double scrollBarPosition, int itemIndexInView)
{
//    qDebug() << "scrollBarPosition: " << scrollBarPosition;
//    qDebug() << "itemIndexInView: " << itemIndexInView;
    m_verticalScrollBarPosition = scrollBarPosition;
    m_itemIndexInView = itemIndexInView;
    emit verticalScrollBarPositionChanged(m_verticalScrollBarPosition, m_itemIndexInView);
}

int BlockModel::getBlockTextLengthWithoutIndentAndDelimiter(int blockIndex) {
    if (blockIndex < 0 || blockIndex > m_blockList.length() - 1)
        return 0;

    QSharedPointer<BlockInfo> blockInfo = m_blockList[blockIndex];
    QString plainText = blockInfo->textPlainText();
    plainText = plainText.mid(blockInfo->indentedString().length()
                              + blockInfo->blockDelimiter().length());
    return plainText.length();
}

void BlockModel::copy(QList<int> selectedBlockIndexes, int firstBlockSelectionStartPos, int lastBlockSelectionEndPos)
{
    std::sort(selectedBlockIndexes.begin(), selectedBlockIndexes.end());
    QString copiedText = "";

    QSharedPointer<BlockInfo> firstBlock = m_blockList[selectedBlockIndexes[0]];
    int lineBreakCount = firstBlock->textPlainText().count("<br />");
    int lineBreaksLength =
            (lineBreakCount * QStringLiteral("<br />").length()) - lineBreakCount;
    int indentAndDelimiterAndLineBreaksLength = firstBlock->indentedString().length() + firstBlock->blockDelimiter().length() + lineBreaksLength;
    if (firstBlockSelectionStartPos == 0 && (selectedBlockIndexes.length() > 1 || lastBlockSelectionEndPos + indentAndDelimiterAndLineBreaksLength == firstBlock->textPlainText().length())) {
        copiedText += firstBlock->textPlainText();
    } else {
        copiedText += firstBlock->textPlainText().mid(indentAndDelimiterAndLineBreaksLength + firstBlockSelectionStartPos);
    }

    if (selectedBlockIndexes.length() > 2) {
        int startLinePos = selectedBlockIndexes[1];
        int endLinePos = selectedBlockIndexes[selectedBlockIndexes.length() - 2];
        QTextBlock startBlock = m_sourceDocument.findBlockByLineNumber(startLinePos);
        QTextBlock endBlock = m_sourceDocument.findBlockByLineNumber(endLinePos);
        QTextCursor cursor(startBlock);
        cursor.setPosition(startBlock.position(), QTextCursor::MoveAnchor);
        cursor.setPosition(endBlock.position() + endBlock.length() - 1, QTextCursor::KeepAnchor);
        copiedText += "\n" + cursor.selectedText().replace("\u2029", "\n");
    }

    if (selectedBlockIndexes.length() > 1) {
        QSharedPointer<BlockInfo> lastBlock = m_blockList[selectedBlockIndexes[selectedBlockIndexes.length() - 1]];
        QString tempLastBlockText = lastBlock->textPlainText();
        tempLastBlockText.replace("<br />", "\n");
        int indentAndDelimiterLastBlock = lastBlock->indentedString().length() + lastBlock->blockDelimiter().length();
        tempLastBlockText.truncate(lastBlockSelectionEndPos + indentAndDelimiterLastBlock);
        copiedText += "\n" + tempLastBlockText;
    }

    copiedText.replace("<br />", "\n");

    m_clipboard->setText(copiedText);
}

void BlockModel::paste(QList<int> selectedBlockIndexes, int firstBlockSelectionStartPos, int lastBlockSelectionEndPos)
{
    std::sort(selectedBlockIndexes.begin(), selectedBlockIndexes.end());
    QString clipboardText = m_clipboard->text();

    if (clipboardText.isEmpty())
        return;


}
