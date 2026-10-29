#pragma once

#include <QAbstractListModel>
#include <QVariantList>

// The book list: a row for every book the query matches, loaded or not. Rows start as blank
// placeholders and fill in as pages arrive, so the list has its full length from the start
// and can jump anywhere with an exact scrollbar. Each row has one role, "book": a map of
// its fields, with "loaded" false for placeholders.
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
    // The book at row, or an empty map if not loaded yet
    QVariantMap book(int row) const;

    // Resets the list to count placeholders
    void reset(int count);
    // Adds placeholders at the end, or removes rows from it, leaving the others in place
    void resize(int count);
    // Fills in rows from first onwards
    void setBooks(int first, const QVariantList &books);
    // Sets one field of a loaded row
    void setField(int row, const QString &name, const QVariant &value);

private:
    QVariantList m_books;
};
