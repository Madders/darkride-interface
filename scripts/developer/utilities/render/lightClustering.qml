//
//  lightClustering.qml
//
//  Created by Sam Gateau on 9/9/2016
//  Copyright 2016 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or https://www.apache.org/licenses/LICENSE-2.0.html
//
//  The frame's light culling (screen tiles and depth bins over the lights in view, as Unity URP's Forward+): its cost,
//  its counts and layout, and two views of what it hands the light loops
import QtQuick 2.5
import QtQuick.Controls 1.4
import "configSlider"
import "../lib/plotperf"

Column {
    spacing: 8
    Column {
        id: lightClustering
        spacing: 10

        Column{
            PlotPerf {
                title: "Light Culling Timing"
                height: 50
                object: Render.getConfig("RenderMainView.LightClustering")
                valueUnit: "ms"
                valueScale: 1
                valueNumDigits: "4"
                plots: [
                    {
                       object: Render.getConfig("RenderMainView.LightClustering"),
                       prop: "cpuRunTime",
                       label: "time",
                       scale: 1,
                       color: "#FFFFFF"
                   }
                ]
            }

            PlotPerf {
                title: "Lights"
                height: 50
                object: Render.getConfig("RenderMainView.LightClustering")
                valueUnit: ""
                valueScale: 1
                valueNumDigits: "0"
                plots: [
                    {
                       object: Render.getConfig("RenderMainView.LightClustering"),
                       prop: "numClusteredLights",
                       label: "in view",
                       color: "#D959FE"
                   },
                   {
                        object: Render.getConfig("RenderMainView.LightClustering"),
                        prop: "numInputLights",
                        label: "input",
                        color: "#FED959"
                    }
                ]
            }

             PlotPerf {
                title: "Scene Lights"
                height: 80
                object: Render.getConfig("RenderMainView.LightClustering")
                valueUnit: ""
                valueScale: 1
                valueNumDigits: "0"
                plots: [
                    {
                       object: Render.getConfig("RenderMainView.LightClustering"),
                       prop: "numSceneLights",
                       label: "current",
                       color: "#00B4EF"
                   },
                   {
                        object: Render.getConfig("RenderMainView.LightClustering"),
                        prop: "numFreeSceneLights",
                        label: "free",
                        color: "#1AC567"
                    },
                   {
                        object: Render.getConfig("RenderMainView.LightClustering"),
                        prop: "numAllocatedSceneLights",
                        label: "allocated",
                        color: "#9495FF"
                    }
                ]
            }

            Label {
                text: {
                    var config = Render.getConfig("RenderMainView.LightClustering");
                    return "Tiles: " + config["tileSize"] + " px, " + config["tileCountX"] + " x " + config["tileCountY"]
                        + "; bins: " + config["binCount"] + "; " + config["wordsPerEntry"] + " word(s) a tile, up to " + config["maxLights"] + " lights";
                }
            }
            Label {
                text:  "Tile bits set = " + Render.getConfig("RenderMainView.LightClustering")["numClusteredLightReferences"].toFixed(0)
            }
            CheckBox {
                    text: "Freeze"
                    checked: Render.getConfig("RenderMainView.LightClustering")["freeze"]
                    onCheckedChanged: { Render.getConfig("RenderMainView.LightClustering")["freeze"] = checked }
            }
            CheckBox {
                    text: "Draw lights per tile and bin (deferred)"
                    checked: Render.getConfig("RenderMainView.DebugLightClusters")["doDrawClusterFromDepth"]
                    onCheckedChanged: { Render.getConfig("RenderMainView.DebugLightClusters")["doDrawClusterFromDepth"] = checked }
            }
            CheckBox {
                    text: "Draw lights per fragment (deferred)"
                    checked: Render.getConfig("RenderMainView.DebugLightClusters")["doDrawContent"]
                    onCheckedChanged: { Render.getConfig("RenderMainView.DebugLightClusters")["doDrawContent"] = checked }
            }
        }
    }
}
