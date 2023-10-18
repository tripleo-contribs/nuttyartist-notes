#ifndef BLOCKMODEL_H
#define BLOCKMODEL_H

#include <algorithm>

#include <QAbstractListModel>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextCursor>
#include <QElapsedTimer>

#include "blockinfo.h"

#include "md4c-html.h"

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
    QTextDocument* sourceDocument();
    static void processOutput(const MD_CHAR* output, MD_SIZE size, void* userdata);
    QString markdownToHtml(const QString &markdown);

public slots:
    void editBlocks(QList<int> selectedBlockIndexes, int firstBlockSelectionStart, int lastBlockSelectionEnd, int savedPressedChar, bool isPressedCharLower);
    void toggleTaskAtIndex(int blockIndex);
    BlockInfo::BlockType getBlockType(int blockIndex);
    void insertNewBlock(int blockIndex, QString plainText);
    void moveBlockTextToPreviousBlock(int blockIndex);
    void backSpaceAtStartOfBlockTextPressed(int blockIndex);
    void indentBlocks(QList<int> selectedBlockIndexes);
    void unindentBlock(unsigned int blockIndex, BlockInfo *blockInfo, bool isSecondRun);
    void unindentBlocks(QList<int> selectedBlockIndexes);
    void setTextAtIndex(const int blockIndex, QString qmlHtml);
    void loadText(const QString& text);
    void clear();

signals:
    void aboutToChangeText();
    void textChangeFinished();
    void aboutToLoadText();
    void loadTextFinished();
    void newBlockCreated(int blockIndex);

private:
    QTextDocument m_sourceDocument;
    QList<BlockInfo*> m_blockList;
    QString m_htmlMetaDataStart;
    QString m_htmlMetaDataEnd;
    unsigned int m_tabLengthInSpaces;
    int m_textLineHeightInPercentage;
    int m_blockIndexToFocusOn;

    void updateBlockUsingPlainText(BlockInfo* blockInfo, unsigned int blockIndex, QString &plainText);
    QString QmlHtmlToMarkdown(QString &qmlHtml);
    void determineBlockIndentAndParentChildRelationship(BlockInfo* blockInfo, int positionToStartSearchFrom);
    void updateBlockText(BlockInfo* blockInfo, const QString &plainText, unsigned int lineStartPos, unsigned int lineEndPos);
    void updateBlocksLinePositions(unsigned int blockPosition, int delta);
    void updateSourceTextBetweenLines(unsigned int startLinePos, unsigned int endLinePos, const QString &newText);
    unsigned int calculateTotalIndentLength(const QString &str, BlockInfo *blockInfo);
};


#endif // BLOCKMODEL_H
