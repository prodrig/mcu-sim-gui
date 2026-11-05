// =============================================================================
// escena3d.qml — la vista 3D (F8 de la CNC): la escena de Qt Quick 3D que
// rellena `Vista3d` (vista3d.cpp). Aquí solo está el escenario -la luz, la
// cámara que se mueve con el ratón- y cómo es un sólido y una superficie;
// cuáles hay y dónde están lo pone C++ con `nuevoSolido` y `nuevaSuperficie`.
//
// Las coordenadas son las de Qt Quick 3D, con Y hacia arriba; las de la
// máquina, con Z hacia arriba, las convierte C++: (x, y, z) -> (x, z, -y).
// Una unidad, un mm.
// =============================================================================
import QtQuick
import QtQuick3D
import QtQuick3D.Helpers

Item {
    id: raiz
    width: 640
    height: 480

    // Desde dónde se mira, y adónde: lo dice la placa, o C++ lo calcula
    function ponCamara(camara, mira) {
        origen.position = mira
        origen.eulerRotation = Qt.vector3d(0, 0, 0)
        camara3d.position = Qt.vector3d(camara.x - mira.x, camara.y - mira.y, camara.z - mira.z)
        camara3d.lookAt(origen.scenePosition)
    }
    function nuevoSolido(props) { return compSolido.createObject(escena, props) }
    function nuevaSuperficie(props) { return compSuperficie.createObject(escena, props) }

    View3D {
        id: vista
        objectName: "vista"
        anchors.fill: parent
        environment: SceneEnvironment {
            clearColor: "#1e272c"
            backgroundMode: SceneEnvironment.Color
            antialiasingMode: SceneEnvironment.MSAA
            antialiasingQuality: SceneEnvironment.Medium
        }

        Node {
            id: origen
            PerspectiveCamera {
                id: camara3d
                clipNear: 2
                clipFar: 20000
                fieldOfView: 40
            }
        }

        DirectionalLight {
            eulerRotation.x: -55
            eulerRotation.y: -35
            brightness: 1.0
            ambientColor: "#505860"
        }
        DirectionalLight {
            eulerRotation.x: -20
            eulerRotation.y: 150
            brightness: 0.35
        }

        Node { id: escena }
    }

    OrbitCameraController {
        anchors.fill: parent
        origin: origen
        camera: camara3d
    }

    // UN SÓLIDO: una caja o un cilindro, ya en ejes de Qt Quick 3D. `orient`
    // pone el eje de un cilindro donde va; `giro` lo gira sobre él, y la
    // marca -una tira oscura en su costado- es lo que deja verlo girar
    Component {
        id: compSolido
        Node {
            property bool cilindro: false
            property vector3d tam: Qt.vector3d(100, 100, 100)
            property vector3d orient: Qt.vector3d(0, 0, 0)
            property real giro: 0
            property color color: "#9e9e9e"
            property bool marca: false
            eulerRotation: orient
            Node {
                eulerRotation.y: giro
                Model {
                    source: cilindro ? "#Cylinder" : "#Cube"
                    scale: Qt.vector3d(tam.x / 100, tam.y / 100, tam.z / 100)
                    materials: PrincipledMaterial {
                        baseColor: color
                        roughness: 0.55
                        metalness: 0.15
                    }
                }
                Model {
                    visible: marca
                    source: "#Cube"
                    position: Qt.vector3d(tam.x / 2, 0, 0)
                    scale: Qt.vector3d(tam.x * 0.3 / 100, tam.y * 0.92 / 100, tam.x * 0.3 / 100)
                    materials: PrincipledMaterial { baseColor: Qt.darker(color, 2.2); roughness: 0.8 }
                }
            }
        }
    }

    // UNA SUPERFICIE: el mapa de alturas de una pieza, con su geometría de C++
    // (`SuperficieGeo`) y el color en cada vértice
    Component {
        id: compSuperficie
        Model {
            materials: DefaultMaterial {
                vertexColorsEnabled: true
                diffuseColor: "white"
                specularAmount: 0.05
            }
        }
    }
}
