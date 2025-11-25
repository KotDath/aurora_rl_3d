// SPDX-FileCopyrightText: 2025 Open Mobile Platform LLC <community@omp.ru>
// SPDX-License-Identifier: BSD-3-Clause

#include "MeshUtils.h"
#include <QString>
#include <QtMath>
#include <QDebug>

MeshData MeshUtils::createUVSphere(int latitudeSegments, int longitudeSegments,
                                   float radius, const QVector4D& color)
{
    validateSegmentCount(latitudeSegments, "UV Sphere latitude segments");
    validateSegmentCount(longitudeSegments, "UV Sphere longitude segments");
    validatePositiveFloat(radius, "radius");

    QVector<Vertex> vertices;

    // Генерируем вершины для сферы
    for (int lat = 0; lat <= latitudeSegments; ++lat) {
        float theta = float(lat) * M_PI / float(latitudeSegments);
        float sinTheta = qSin(theta);
        float cosTheta = qCos(theta);

        for (int lon = 0; lon <= longitudeSegments; ++lon) {
            float phi = float(lon) * 2.0f * M_PI / float(longitudeSegments);
            float sinPhi = qSin(phi);
            float cosPhi = qCos(phi);

            QVector3D position;
            position.setX(radius * sinTheta * cosPhi);
            position.setY(radius * cosTheta);
            position.setZ(radius * sinTheta * sinPhi);

            // Для сферы нормаль равна нормализованной позиции
            QVector3D normal = position.normalized();

            vertices << Vertex(position, color, normal);
        }
    }

    // Генерируем треугольники с нормалями
    QVector<Vertex> triangleVertices;
    for (int lat = 0; lat < latitudeSegments; ++lat) {
        for (int lon = 0; lon < longitudeSegments; ++lon) {
            int first = lat * (longitudeSegments + 1) + lon;
            int second = first + longitudeSegments + 1;

            // Первый треугольник
            triangleVertices << vertices[first];
            triangleVertices << vertices[second];
            triangleVertices << vertices[first + 1];

            // Второй треугольник
            triangleVertices << vertices[second];
            triangleVertices << vertices[second + 1];
            triangleVertices << vertices[first + 1];
        }
    }

    return MeshData(triangleVertices, GL_TRIANGLES);
}

MeshData MeshUtils::createCylinder(int radialSegments, float radius, float height,
                                   const QVector4D& color, bool includeCaps)
{
    validateSegmentCount(radialSegments, "Cylinder radial segments");
    validatePositiveFloat(radius, "radius");
    validatePositiveFloat(height, "height");

    QVector<Vertex> vertices;

    // Генерируем вершины для боковой поверхности цилиндра
    float halfHeight = height * 0.5f;

    // Верхние и нижние вершины с нормалями
    for (int i = 0; i <= radialSegments; ++i) {
        float angle = float(i) * 2.0f * M_PI / float(radialSegments);
        float x = radius * qCos(angle);
        float z = radius * qSin(angle);

        // Нормаль для боковой поверхности (направлена от оси цилиндра)
        QVector3D sideNormal = QVector3D(x, 0.0f, z).normalized();

        // Нижняя вершина
        vertices << Vertex(QVector3D(x, -halfHeight, z), color, sideNormal);
        // Верхняя вершина
        vertices << Vertex(QVector3D(x, halfHeight, z), color, sideNormal);
    }

    // Генерируем треугольники для боковой поверхности
    QVector<Vertex> triangleVertices;
    for (int i = 0; i < radialSegments; ++i) {
        int current = i * 2;
        int next = current + 2;

        // Первый треугольник
        triangleVertices << vertices[current];
        triangleVertices << vertices[next];
        triangleVertices << vertices[current + 1];

        // Второй треугольник
        triangleVertices << vertices[next];
        triangleVertices << vertices[next + 1];
        triangleVertices << vertices[current + 1];
    }

    // Добавляем крышки, если необходимо
    if (includeCaps) {
        // Центры крышек с нормалями
        QVector3D bottomCenter(0.0f, -halfHeight, 0.0f);
        QVector3D topCenter(0.0f, halfHeight, 0.0f);
        QVector3D bottomNormal(0.0f, -1.0f, 0.0f);  // Направлена вниз
        QVector3D topNormal(0.0f, 1.0f, 0.0f);     // Направлена вверх

        // Генерируем треугольники для нижней крышки
        for (int i = 0; i < radialSegments; ++i) {
            int current = i * 2;
            int next = (i + 1) % radialSegments * 2;

            triangleVertices << Vertex(bottomCenter, color, bottomNormal);
            triangleVertices << Vertex(vertices[next].position, color, bottomNormal);
            triangleVertices << Vertex(vertices[current].position, color, bottomNormal);
        }

        // Генерируем треугольники для верхней крышки
        for (int i = 0; i < radialSegments; ++i) {
            int current = i * 2 + 1;
            int next = ((i + 1) % radialSegments) * 2 + 1;

            triangleVertices << Vertex(topCenter, color, topNormal);
            triangleVertices << Vertex(vertices[current].position, color, topNormal);
            triangleVertices << Vertex(vertices[next].position, color, topNormal);
        }
    }

    return MeshData(triangleVertices, GL_TRIANGLES);
}

MeshData MeshUtils::createCube(float width, float height, float depth,
                              const QVector4D& color)
{
    validatePositiveFloat(width, "width");
    validatePositiveFloat(height, "height");
    validatePositiveFloat(depth, "depth");

    QVector<Vertex> vertices;

    // Вычисляем половинные размеры для центрирования
    float halfWidth = width * 0.5f;
    float halfHeight = height * 0.5f;
    float halfDepth = depth * 0.5f;

    // Определяем 8 вершин куба
    QVector3D v0(-halfWidth, -halfHeight, -halfDepth); // 0 - левый нижний задний
    QVector3D v1(halfWidth, -halfHeight, -halfDepth);  // 1 - правый нижний задний
    QVector3D v2(halfWidth, halfHeight, -halfDepth);   // 2 - правый верхний задний
    QVector3D v3(-halfWidth, halfHeight, -halfDepth);  // 3 - левый верхний задний
    QVector3D v4(-halfWidth, -halfHeight, halfDepth);  // 4 - левый нижний передний
    QVector3D v5(halfWidth, -halfHeight, halfDepth);   // 5 - правый нижний передний
    QVector3D v6(halfWidth, halfHeight, halfDepth);    // 6 - правый верхний передний
    QVector3D v7(-halfWidth, halfHeight, halfDepth);   // 7 - левый верхний передний

    // Передняя грань (Z+) - нормаль вперед
    QVector3D frontNormal(0.0f, 0.0f, 1.0f);
    addTriangle(vertices, v4, v5, v6, color, frontNormal);
    addTriangle(vertices, v4, v6, v7, color, frontNormal);

    // Задняя грань (Z-) - нормаль назад
    QVector3D backNormal(0.0f, 0.0f, -1.0f);
    addTriangle(vertices, v1, v0, v3, color, backNormal);
    addTriangle(vertices, v1, v3, v2, color, backNormal);

    // Левая грань (X-) - нормаль влево
    QVector3D leftNormal(-1.0f, 0.0f, 0.0f);
    addTriangle(vertices, v0, v4, v7, color, leftNormal);
    addTriangle(vertices, v0, v7, v3, color, leftNormal);

    // Правая грань (X+) - нормаль вправо
    QVector3D rightNormal(1.0f, 0.0f, 0.0f);
    addTriangle(vertices, v5, v1, v2, color, rightNormal);
    addTriangle(vertices, v5, v2, v6, color, rightNormal);

    // Верхняя грань (Y+) - нормаль вверх
    QVector3D topNormal(0.0f, 1.0f, 0.0f);
    addTriangle(vertices, v7, v6, v2, color, topNormal);
    addTriangle(vertices, v7, v2, v3, color, topNormal);

    // Нижняя грань (Y-) - нормаль вниз
    QVector3D bottomNormal(0.0f, -1.0f, 0.0f);
    addTriangle(vertices, v0, v1, v5, color, bottomNormal);
    addTriangle(vertices, v0, v5, v4, color, bottomNormal);

    return MeshData(vertices, GL_TRIANGLES);
}

MeshData MeshUtils::createPlane(float width, float depth,
                               int widthSegments, int depthSegments,
                               const QVector4D& color)
{
    validatePositiveFloat(width, "width");
    validatePositiveFloat(depth, "depth");
    validateSegmentCount(widthSegments, "Plane width segments");
    validateSegmentCount(depthSegments, "Plane depth segments");

    QVector<Vertex> vertices;

    float halfWidth = width * 0.5f;
    float halfDepth = depth * 0.5f;

    // Генерируем вершины сетки
    QVector<QVector<QVector3D>> grid;
    for (int z = 0; z <= depthSegments; ++z) {
        QVector<QVector3D> row;
        float zPos = -halfDepth + (float(z) * depth / float(depthSegments));

        for (int x = 0; x <= widthSegments; ++x) {
            float xPos = -halfWidth + (float(x) * width / float(widthSegments));
            row << QVector3D(xPos, 0.0f, zPos);
        }
        grid << row;
    }

    // Генерируем треугольники с нормалями (направлены вверх)
    QVector3D planeNormal(0.0f, 1.0f, 0.0f);  // Нормаль вверх
    for (int z = 0; z < depthSegments; ++z) {
        for (int x = 0; x < widthSegments; ++x) {
            // Получаем четыре вершины текущего квадрата
            QVector3D v1 = grid[z][x];       // левый нижний
            QVector3D v2 = grid[z][x + 1];   // правый нижний
            QVector3D v3 = grid[z + 1][x + 1]; // правый верхний
            QVector3D v4 = grid[z + 1][x];   // левый верхний

            // Первый треугольник
            addTriangle(vertices, v1, v2, v3, color, planeNormal);

            // Второй треугольник
            addTriangle(vertices, v1, v3, v4, color, planeNormal);
        }
    }

    return MeshData(vertices, GL_TRIANGLES);
}

void MeshUtils::addTriangle(QVector<Vertex>& vertices,
                           const QVector3D& v1, const QVector3D& v2, const QVector3D& v3,
                           const QVector4D& color, const QVector3D& normal)
{
    vertices << Vertex(v1, color, normal);
    vertices << Vertex(v2, color, normal);
    vertices << Vertex(v3, color, normal);
}

void MeshUtils::validateSegmentCount(int segments, const QString& shapeName)
{
    if (segments < 1) {
        qWarning() << "MeshUtils::" << shapeName << ": segments must be at least 1, got" << segments;
        segments = 1;
    }
}

void MeshUtils::validatePositiveFloat(float value, const QString& paramName)
{
    if (value <= 0.0f) {
        qWarning() << "MeshUtils: " << paramName << "must be positive, got" << value;
    }
}