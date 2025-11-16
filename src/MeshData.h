// SPDX-FileCopyrightText: 2025 Open Mobile Platform LLC <community@omp.ru>
// SPDX-License-Identifier: BSD-3-Clause

#ifndef MESHDATA_H
#define MESHDATA_H

#include <QVector3D>
#include <QVector4D>
#include <QVector>
#include <QtOpenGL/QGLFunctions>

struct Vertex
{
    QVector3D position;
    QVector4D color;

    Vertex() {}
    Vertex(const QVector3D& pos, const QVector4D& col)
        : position(pos), color(col) {}
};

class MeshData
{
public:
    MeshData();
    MeshData(const QVector<Vertex>& vertices, GLenum primitiveType = GL_TRIANGLES);

    const QVector<Vertex>& vertices() const { return m_vertices; }
    void setVertices(const QVector<Vertex>& vertices) { m_vertices = vertices; }

    GLenum primitiveType() const { return m_primitiveType; }
    void setPrimitiveType(GLenum type) { m_primitiveType = type; }

    int vertexCount() const { return m_vertices.size(); }

    static MeshData createTriangle(const QVector3D& p1, const QVector3D& p2, const QVector3D& p3,
                                  const QVector4D& color = QVector4D(1.0f, 1.0f, 1.0f, 1.0f));
    static MeshData createColoredTriangle(const QVector3D& p1, const QVector3D& p2, const QVector3D& p3,
                                          const QVector4D& color1, const QVector4D& color2, const QVector4D& color3);

    static MeshData createColoredCube(float size = 1.0f);
    static MeshData createColoredCube(float size, const QVector4D& baseColor);
    static MeshData createPlane(float width, float depth, const QVector4D& color);

private:
    QVector<Vertex> m_vertices;
    GLenum m_primitiveType;
};

#endif // MESHDATA_H
