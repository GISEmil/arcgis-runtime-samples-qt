// [WriteFile Name=GraphicsOverlayDictionaryRenderer, Category=DisplayInformation]
// [Legal]
// Copyright 2016 Esri.

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// [Legal]

#include "GraphicsOverlayDictionaryRenderer.h"

#include <QQmlProperty>

#include "DictionaryRenderer.h"
#include "GeometryEngine.h"
#include "GraphicListModel.h"
#include "Map.h"
#include "MapQuickView.h"
#include "PolygonBuilder.h"
#include "MultipointBuilder.h"
#include "DictionarySymbolStyle.h"

using namespace Esri::ArcGISRuntime;

const QString GraphicsOverlayDictionaryRenderer::FIELD_CONTROL_POINTS = "_control_points";
const QString GraphicsOverlayDictionaryRenderer::FIELD_WKID = "_wkid";

GraphicsOverlayDictionaryRenderer::GraphicsOverlayDictionaryRenderer(QQuickItem* parent) :
    QQuickItem(parent),
    m_map(nullptr),
    m_mapView(nullptr),
    m_graphicsOverlay(nullptr)
{

}

GraphicsOverlayDictionaryRenderer::~GraphicsOverlayDictionaryRenderer()
{

}

void GraphicsOverlayDictionaryRenderer::componentComplete()
{
    QQuickItem::componentComplete();

    // QML properties
    m_dataPath = QQmlProperty::read(this, "dataPath").toUrl().toLocalFile();
    
    //! [Apply Dictionary Renderer Graphics Overlay Cpp]
    // Create graphics overlay
    m_graphicsOverlay = new GraphicsOverlay(this);

    // Create dictionary renderer and apply to the graphics overlay
    DictionarySymbolStyle* dictionarySymbolStyle = new DictionarySymbolStyle("mil2525d", m_dataPath + "/styles/mil2525d.stylx", this);
    DictionaryRenderer* renderer = new DictionaryRenderer(dictionarySymbolStyle, this);
    m_graphicsOverlay->setRenderer(renderer);
    //! [Apply Dictionary Renderer Graphics Overlay Cpp]

    // Create a map and give it to the MapView
    m_mapView = findChild<MapQuickView*>("mapView");
    m_map = new Map(Basemap::topographic(this), this);
    m_mapView->setMap(m_map);
    m_mapView->graphicsOverlays()->append(m_graphicsOverlay);

    parseXmlFile();
    emit graphicsLoaded();
}

void GraphicsOverlayDictionaryRenderer::parseXmlFile()
{
    bool readingMessage = false;
    QVariantMap elementValues;
    QString currentElementName;

    QFile xmlFile(m_dataPath + "/xml/Mil2525DMessages.xml");
    // Open the file for reading
    if (xmlFile.isOpen())
    {
        xmlFile.reset();
    }
    else
    {
        xmlFile.open(QIODevice::ReadOnly | QIODevice::Text);
    }
    m_xmlParser.setDevice(&xmlFile);

    // Traverse the XML in a loop
    while (!m_xmlParser.atEnd())
    {
        m_xmlParser.readNext();

        // Is this the start or end of a message element?
        if (m_xmlParser.name() == "message")
        {
            if (!readingMessage)
            {
                // This is the start of a message element.
                elementValues.clear();
            }
            else
            {
                /**
                 * This is the end of a message element. Here we have a complete message that defines
                 * a military feature to display on the map. Create a graphic from its attributes.
                 */
                createGraphic(elementValues);
            }
            // Either we just started reading a message, or we just finished reading a message.
            readingMessage = !readingMessage;
        }
        // Are we already inside a message element?
        else if (readingMessage)
        {
            // Is this the start of an element inside a message?
            if (m_xmlParser.isStartElement())
            {
                // Remember which element we're reading
                currentElementName = m_xmlParser.name().toString();
            }
            // Is this text?
            else if (m_xmlParser.isCharacters())
            {
                // Is this text inside an element?
                if (!currentElementName.isEmpty())
                {
                    // Get the text and store it as the current element's value
                    QStringRef trimmedText = m_xmlParser.text().trimmed();
                    if (!trimmedText.isEmpty())
                    {
                        elementValues[currentElementName] = trimmedText.toString();
                    }
                }
            }
        }
    }
}

void GraphicsOverlayDictionaryRenderer::createGraphic(QVariantMap rawAttributes)
{
    // If _wkid was absent, use WGS 1984 (4326) by default.
    int wkid = rawAttributes.keys().contains(FIELD_WKID) ? rawAttributes[FIELD_WKID].toInt() : 4326;
    SpatialReference sr(wkid);
    Geometry geom;
    QStringList pointStrings = rawAttributes[FIELD_CONTROL_POINTS].toString().split(";");
    if (pointStrings.length() == 1)
    {
        // It's a point
        QStringList coords = pointStrings[0].split(",");
        geom = Point(coords[0].toDouble(), coords[1].toDouble(), sr);
    }
    else {
        // It's a multipoint
        auto builder = new MultipointBuilder(sr, this);
        PointCollection* collection = new PointCollection(sr, this);
        foreach (auto pointString, pointStrings)
        {
            QStringList coords = pointString.split(",");
            if (coords.length() >= 2)
                collection->addPoint(coords[0].toDouble(), coords[1].toDouble());
        }
        builder->setPoints(collection);
        geom = builder->toGeometry();
    }

    if (!geom.isEmpty())
    {

        // Get rid of _control_points and _wkid. They are not needed in the graphic's
        // attributes.
        rawAttributes.remove(FIELD_CONTROL_POINTS);
        rawAttributes.remove(FIELD_WKID);

        Graphic* graphic = new Graphic(geom, rawAttributes, this);
        m_graphicsOverlay->graphics()->append(graphic);
        m_bbox = m_bbox.isEmpty() ? graphic->geometry().extent() : GeometryEngine::unionOf(m_bbox.extent(),  graphic->geometry().extent()).extent();
    }
}

void GraphicsOverlayDictionaryRenderer::zoomToGraphics()
{
    m_mapView->setViewpointGeometry(m_bbox.extent(), 20);
}
