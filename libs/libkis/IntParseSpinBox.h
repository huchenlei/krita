/*
 * SPDX-FileCopyrightText: 2016 Laurent Valentin Jospin <laurent.valentin@famillejospin.ch>
 * SPDX-FileCopyrightText: 2021 Deif Lou <ginoba@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef LIBKIS_INTPARSESPINBOX_H
#define LIBKIS_INTPARSESPINBOX_H

#include "kis_int_parse_spin_box.h"

#include "kritalibkis_export.h"
#include "libkis.h"

/**
 * @brief The IntParseSpinBox class is a cleverer SpinBox, able to parse arithmetic expressions.
 *
 * Use this spinbox instead of the basic one from Qt if you want it to be able to parse arithmetic expressions.
 */
class KRITALIBKIS_EXPORT IntParseSpinBox : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(IntParseSpinBox)

public:
    explicit IntParseSpinBox();
    ~IntParseSpinBox() override;

public Q_SLOTS:

    QSpinBox* widget() const;

    /**
     * @brief This is a reimplementation of @ref QSpinBox::stepBy that
     * uses @ref setValue
     * @param steps Number of steps that the value should change
     */
    void stepBy(int steps);
    /**
     * @brief Set the value of the spinbox
     * 
     * This reimplementation also tries to clear the current expression and
     * warning message whenever possible. This will happen when the new value
     * is different from the current one and the line edit has not the focus
     * or it is read-only. One can force the reset also by passing true to the
     * @p overwriteExpression parameter.
     * 
     * @param value The new value
     * @param overwriteExpression Get if the expression in the edit field
     * (and the warning message) should be reset to reflect the new value.
     * The default is false so that if the user is editing the expression
     * it won't be disrupted by any default call to this function
     */
    void setValue(int value, bool overwriteExpression = false);
    /**
     * @brief Get if the last expression entered is a valid one
     * @retval true if the last expression entered is valid
     * @retval false otherwise
     */
    bool isLastValid() const;
    /**
     * @brief This virtual function is similar to cleanText(). But child classes
     * may reimplement it to further process ("clean up") the expression.
     * @return The processed expression
     */
    virtual QString veryCleanText() const;

Q_SIGNALS:
    /**
     * @brief signal emitted when the last parsed expression is not valid.
     */
    void errorWhileParsing(const QString &expr) const;
    /**
     * @brief signal emitted when the last parsed expression is valid and
     * the expression before was not valid.
     */
    void noMoreParsingError() const;

private:
    struct Private;
    Private *const d;

};

#endif // LIBKIS_INTPARSESPINBOX_H
