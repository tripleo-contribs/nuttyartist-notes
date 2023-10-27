#ifndef BLOCKMODEL_H
#define BLOCKMODEL_H

#include <algorithm>

#include <QAbstractListModel>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextCursor>
#include <QElapsedTimer>
#include <QSharedPointer>

#include "blockinfo.h"

#include "md4c-html.h"

enum class OneCharOperation {
    CharInsert,
    CharDelete,
    Indent,
    Unindent,
    NoOneCharOperation,
};

enum ActionType { Insert, Remove, Modify };

struct SingleAction
{
    unsigned int blockStartIndex = 0;
    unsigned int blockEndIndex = 0;
    ActionType actionType = ActionType::Modify;
    QString oldPlainText = "";
    QString newPlainText = "";
    OneCharOperation oneCharOperation = OneCharOperation::NoOneCharOperation;
    int lastCursorPosition = 0;
};

struct CompoundAction
{
    QList<SingleAction> actions;
};

class BlockModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        BlockTextHtmlRole = Qt::UserRole + 1,
        BlockTextPlainTextRole,
        BlockTypeRole,
        BlockLineStartPosRole,
        BlockLineEndPosRole,
        BlockTotalIndentLengthRole,
        BlockIndentLevelRole,
        BlockDelimiterRole,
        BlockChildrenRole,
        BlockMetaData,
    };

    explicit BlockModel(QObject *parent = nullptr);

    virtual int rowCount(const QModelIndex &parent) const override;
    virtual QVariant data(const QModelIndex &index, int role) const override;
    virtual QHash<int, QByteArray> roleNames() const override;
    QTextDocument *sourceDocument();
    static void processOutput(const MD_CHAR *output, MD_SIZE size, void *userdata);
    QString markdownToHtml(const QString &markdown);

public slots:
    void undo();
    void redo();
    void editBlocks(QList<int> selectedBlockIndexes, int firstBlockSelectionStart,
                    int lastBlockSelectionEnd, int savedPressedChar, bool isPressedCharLower);
    void toggleTaskAtIndex(int blockIndex);
    BlockInfo::BlockType getBlockType(int blockIndex);
    void insertNewBlock(int blockIndex, QString plainText,
                        bool shouldMergeWithPreviousAction = false);
    void moveBlockTextToBlockAbove(int blockIndex);
    void backSpacePressedAtStartOfBlock(int blockIndex);
    void indentBlocks(QList<int> selectedBlockIndexes);
    void unindentBlock(unsigned int blockIndex, QSharedPointer<BlockInfo> &blockInfo,
                       bool isSecondRun, int numberOfAlreadyUnindentedBlocks);
    void unindentBlocks(QList<int> selectedBlockIndexes);
    void setTextAtIndex(const int blockIndex, QString qmlHtml, int cursorPositionQML = 0);
    void loadText(const QString &text);
    void clear();

signals:
    void blockToFocusOnChanged(int blockIndex);
    void aboutToChangeText();
    void textChangeFinished();
    void aboutToLoadText();
    void loadTextFinished();
    void newBlockCreated(int blockIndex);

private:
    QTextDocument m_sourceDocument;
    QList<QSharedPointer<BlockInfo>> m_blockList;
    QString m_htmlMetaDataStart;
    QString m_htmlMetaDataEnd;
    unsigned int m_tabLengthInSpaces;
    int m_textLineHeightInPercentage;
    int m_blockIndexToFocusOn;
    QList<CompoundAction> m_undoStack;
    QList<CompoundAction> m_redoStack;

    void updateBlockUsingPlainText(QSharedPointer<BlockInfo> &blockInfo, unsigned int blockIndex,
                                   QString &plainText);
    QString QmlHtmlToMarkdown(QString &qmlHtml);
    void determineBlockIndentAndParentChildRelationship(QSharedPointer<BlockInfo> &blockInfo,
                                                        int positionToStartSearchFrom);
    void updateBlockText(QSharedPointer<BlockInfo> &blockInfo, const QString &plainText,
                         unsigned int lineStartPos, unsigned int lineEndPos);
    void updateBlocksLinePositions(unsigned int blockPosition, int delta);
    void updateSourceTextBetweenLines(
            int startLinePos, int endLinePos, const QString &newText, bool shouldCreateUndo = true,
            int cursorPosition = 0, ActionType actionType = ActionType::Modify,
            OneCharOperation oneCharoperation = OneCharOperation::NoOneCharOperation,
            bool isForceMergeLastAction = false);
    unsigned int calculateTotalIndentLength(const QString &str,
                                            QSharedPointer<BlockInfo> &blockInfo);
    double estimateMemoryUsageInKB(const QList<CompoundAction> &undoStack);
};

#endif // BLOCKMODEL_H
