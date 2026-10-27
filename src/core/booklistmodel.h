#pragma once

#include <QAbstractListModel>
#include <QVariantList>

// The book list: one row for every book the query matches, loaded or not. Rows start as
// blank placeholders and are filled in as their pages arrive, so the list is full length
// from the start: it can jump to any book, and its scrollbar is exact.
// Each row has one role, "book": the book's fields as a map, with "loaded" false for a
// placeholder.
class BookListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return m_books.size(); }
    bool isLoaded(int row) const;
    // The book at row, or an empty map while it isn't loaded
    QVariantMap book(int row) const;

    // Replaces the list with count placeholders
    void reset(int count);
    // Fills in rows from first onwards
    void setBooks(int first, const QVariantList &books);
    // Changes one field of a loaded row
    void setField(int row, const QString &name, const QVariant &value);

private:
    QVariantList m_books;
};
