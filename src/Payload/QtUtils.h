/*
 *  Lauss - PoC blocking ad banners in LINE clients on Windows
 *  Copyright (C) 2023-2026 Mifan Bang <https://debug.tw>.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "QtInterface.h"

#include <optional>
#include <utility>
#include <vector>


// ------------------------------------------------------------------------------
// QObject, QMetaObject, etc.

const char* GetQtClassName(const QObject& object);


// ------------------------------------------------------------------------------
// Signal, connections, etc.

using IndexedMethod = std::pair<int, QMetaMethod>;
std::vector<IndexedMethod> GetMethods(const QObject& object);

std::optional<std::vector<Connection*>> GetConnections(QObject& object, const char* signal);

bool PrintConnectionsToSignal(QObject& object, const char* signal);

void PrintMethodsWithSignalConnections(QObject& object);


// ------------------------------------------------------------------------------
// QWidget, etc.

std::vector<QWidget*> FindOwningWidgets(QWidget& bottomWidget);  // Inclusive of the bottom-most widget

std::vector<QWidget*> FindBanners();


// ------------------------------------------------------------------------------
// High level operations specific to Lauss

bool HideBanner(QWidget& bannerWidget);
