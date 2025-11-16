// SPDX-FileCopyrightText: 2025 Open Mobile Platform LLC <community@omp.ru>
// SPDX-License-Identifier: BSD-3-Clause

#ifndef SCENETYPES_H
#define SCENETYPES_H

#include <QMatrix4x4>

struct SceneRenderInstance
{
    int meshId = -1;
    QMatrix4x4 modelMatrix;
    bool visible = true;
};

#endif // SCENETYPES_H
