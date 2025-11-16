// SPDX-FileCopyrightText: 2025 Open Mobile Platform LLC <community@omp.ru>
// SPDX-License-Identifier: BSD-3-Clause

#include "MeshData.h"

#include <QtOpenGL/QGLFunctions>

MeshData::MeshData()
    : m_primitiveType(GL_TRIANGLES)
{
}

MeshData::MeshData(const QVector<Vertex>& vertices, GLenum primitiveType)
    : m_vertices(vertices)
    , m_primitiveType(primitiveType)
{
}

MeshData MeshData::createTriangle(const QVector3D& p1, const QVector3D& p2, const QVector3D& p3,
                                  const QVector4D& color)
{
    QVector<Vertex> vertices;
    vertices << Vertex(p1, color);
    vertices << Vertex(p2, color);
    vertices << Vertex(p3, color);

    return MeshData(vertices, GL_TRIANGLES);
}

MeshData MeshData::createColoredTriangle(const QVector3D& p1, const QVector3D& p2, const QVector3D& p3,
                                          const QVector4D& color1, const QVector4D& color2, const QVector4D& color3)
{
    QVector<Vertex> vertices;
    vertices << Vertex(p1, color1);
    vertices << Vertex(p2, color2);
    vertices << Vertex(p3, color3);

    return MeshData(vertices, GL_TRIANGLES);
}

MeshData MeshData::createColoredCube(float size)
{
    QVector<Vertex> vertices;
    float s = size / 2.0f;

    // Определяем вершины куба
    QVector3D v0(-s, -s, -s); // 0
    QVector3D v1( s, -s, -s); // 1
    QVector3D v2( s,  s, -s); // 2
    QVector3D v3(-s,  s, -s); // 3
    QVector3D v4(-s, -s,  s); // 4
    QVector3D v5( s, -s,  s); // 5
    QVector3D v6( s,  s,  s); // 6
    QVector3D v7(-s,  s,  s); // 7

    // Цвета для каждой грани
    QVector4D frontColor(1.0f, 0.0f, 0.0f, 1.0f);   // Красная - передняя
    QVector4D backColor(0.0f, 1.0f, 0.0f, 1.0f);    // Зеленая - задняя
    QVector4D leftColor(0.0f, 0.0f, 1.0f, 1.0f);    // Синяя - левая
    QVector4D rightColor(1.0f, 1.0f, 0.0f, 1.0f);   // Желтая - правая
    QVector4D topColor(1.0f, 0.0f, 1.0f, 1.0f);     // Магента - верхняя
    QVector4D bottomColor(0.0f, 1.0f, 1.0f, 1.0f);  // Циановая - нижняя

    // Передняя грань (Z+)
    vertices << Vertex(v4, frontColor) << Vertex(v5, frontColor) << Vertex(v6, frontColor);
    vertices << Vertex(v4, frontColor) << Vertex(v6, frontColor) << Vertex(v7, frontColor);

    // Задняя грань (Z-)
    vertices << Vertex(v1, backColor) << Vertex(v0, backColor) << Vertex(v3, backColor);
    vertices << Vertex(v1, backColor) << Vertex(v3, backColor) << Vertex(v2, backColor);

    // Левая грань (X-)
    vertices << Vertex(v0, leftColor) << Vertex(v4, leftColor) << Vertex(v7, leftColor);
    vertices << Vertex(v0, leftColor) << Vertex(v7, leftColor) << Vertex(v3, leftColor);

    // Правая грань (X+)
    vertices << Vertex(v5, rightColor) << Vertex(v1, rightColor) << Vertex(v2, rightColor);
    vertices << Vertex(v5, rightColor) << Vertex(v2, rightColor) << Vertex(v6, rightColor);

    // Верхняя грань (Y+)
    vertices << Vertex(v7, topColor) << Vertex(v6, topColor) << Vertex(v2, topColor);
    vertices << Vertex(v7, topColor) << Vertex(v2, topColor) << Vertex(v3, topColor);

    // Нижняя грань (Y-)
    vertices << Vertex(v0, bottomColor) << Vertex(v1, bottomColor) << Vertex(v5, bottomColor);
    vertices << Vertex(v0, bottomColor) << Vertex(v5, bottomColor) << Vertex(v4, bottomColor);

    return MeshData(vertices, GL_TRIANGLES);
}

MeshData MeshData::createColoredCube(float size, const QVector4D& baseColor)
{
    QVector<Vertex> vertices;
    float s = size / 2.0f;

    // Определяем вершины куба
    QVector3D v0(-s, -s, -s); // 0
    QVector3D v1( s, -s, -s); // 1
    QVector3D v2( s,  s, -s); // 2
    QVector3D v3(-s,  s, -s); // 3
    QVector3D v4(-s, -s,  s); // 4
    QVector3D v5( s, -s,  s); // 5
    QVector3D v6( s,  s,  s); // 6
    QVector3D v7(-s,  s,  s); // 7

    // Модифицируем базовый цвет для каждой грани
    QVector4D frontColor = baseColor * QVector4D(1.0f, 0.8f, 0.8f, 1.0f);   // Более светлый для передней
    QVector4D backColor = baseColor * QVector4D(0.8f, 1.0f, 0.8f, 1.0f);    // Более зеленоватый для задней
    QVector4D leftColor = baseColor * QVector4D(0.8f, 0.8f, 1.0f, 1.0f);    // Более синеватый для левой
    QVector4D rightColor = baseColor * QVector4D(1.0f, 1.0f, 0.8f, 1.0f);   // Более желтоватый для правой
    QVector4D topColor = baseColor * QVector4D(1.0f, 0.8f, 1.0f, 1.0f);     // Более розоватый для верхней
    QVector4D bottomColor = baseColor * QVector4D(0.8f, 1.0f, 1.0f, 1.0f);  // Более циановый для нижней

    // Передняя грань (Z+)
    vertices << Vertex(v4, frontColor) << Vertex(v5, frontColor) << Vertex(v6, frontColor);
    vertices << Vertex(v4, frontColor) << Vertex(v6, frontColor) << Vertex(v7, frontColor);

    // Задняя грань (Z-)
    vertices << Vertex(v1, backColor) << Vertex(v0, backColor) << Vertex(v3, backColor);
    vertices << Vertex(v1, backColor) << Vertex(v3, backColor) << Vertex(v2, backColor);

    // Левая грань (X-)
    vertices << Vertex(v0, leftColor) << Vertex(v4, leftColor) << Vertex(v7, leftColor);
    vertices << Vertex(v0, leftColor) << Vertex(v7, leftColor) << Vertex(v3, leftColor);

    // Правая грань (X+)
    vertices << Vertex(v5, rightColor) << Vertex(v1, rightColor) << Vertex(v2, rightColor);
    vertices << Vertex(v5, rightColor) << Vertex(v2, rightColor) << Vertex(v6, rightColor);

    // Верхняя грань (Y+)
    vertices << Vertex(v7, topColor) << Vertex(v6, topColor) << Vertex(v2, topColor);
    vertices << Vertex(v7, topColor) << Vertex(v2, topColor) << Vertex(v3, topColor);

    // Нижняя грань (Y-)
    vertices << Vertex(v0, bottomColor) << Vertex(v1, bottomColor) << Vertex(v5, bottomColor);
    vertices << Vertex(v0, bottomColor) << Vertex(v5, bottomColor) << Vertex(v4, bottomColor);

    return MeshData(vertices, GL_TRIANGLES);
}