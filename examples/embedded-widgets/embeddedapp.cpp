/*
 * SPDX-FileCopyrightText: (C) 2014 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "embeddedapp.h"

#include "root.h"

#include <QDebug>
#include <QCoreApplication>

#include <Cutelyst/Plugins/Session/Session>

EmbeddedApp::EmbeddedApp(QObject *parent) :
    Cutelyst::Application(parent)
{
}

EmbeddedApp::~EmbeddedApp()
{
}

bool EmbeddedApp::init()
{
    qDebug() << "EmbeddedApp::init()";

    auto root = new Root(this);
    connect(root, &Root::indexCalled, this, &EmbeddedApp::indexCalled);

    return true;
}
