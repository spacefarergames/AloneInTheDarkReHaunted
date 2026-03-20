///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Debug visualization tools and 3D wireframe display
///////////////////////////////////////////////////////////////////////////////


#include <vector>
#include "common.h"
#include "postProcessing.h"

#ifdef FITD_DEBUGGER

bool debuggerVar_debugMenuDisplayed = false;

////// debug var used in engine
bool debuggerVar_drawModelZv = false;
bool debuggerVar_drawCameraCoverZone = false;
bool debuggerVar_noHardClip = false;
bool debuggerVar_topCamera = false;
long int debufferVar_topCameraZoom = -4000;

bool debuggerVar_useBlackBG = false;
bool debuggerVar_fastForward = false;
///////////////////////////////

u8 debugger_mainDebugButtonVar_toggleDrawModelZv = 0;
u8 debugger_mainDebugButtonVar_toggleDrawCameraCoverZone = 0;

void SaveScene()
{
    std::vector<float> vertices;
    std::vector<int> indices;

    int numRooms = getNumberOfRoom();
    for (int i = 0; i < numRooms; i++)
    {
        roomDataStruct* pRoomData = &roomDataTable[i];

        for (int j = 0; j < pRoomData->numHardCol; j++)
        {
            hardColStruct* pHardCol = &pRoomData->hardColTable[j];

            float ZVX1 = ((pHardCol->zv.ZVX1 + pRoomData->worldX * 10));
            float ZVX2 = ((pHardCol->zv.ZVX2 + pRoomData->worldX * 10));
            float ZVY1 = -((pHardCol->zv.ZVY1 - pRoomData->worldY * 10));
            float ZVY2 = -((pHardCol->zv.ZVY2 - pRoomData->worldY * 10));
            float ZVZ1 = -((pHardCol->zv.ZVZ1 - pRoomData->worldZ * 10));
            float ZVZ2 = -((pHardCol->zv.ZVZ2 - pRoomData->worldZ * 10));

            int firstIndex = vertices.size() / 3 + 1;
            vertices.push_back(ZVX1); vertices.push_back(ZVY1); vertices.push_back(ZVZ1);
            vertices.push_back(ZVX2); vertices.push_back(ZVY1); vertices.push_back(ZVZ1);
            vertices.push_back(ZVX2); vertices.push_back(ZVY1); vertices.push_back(ZVZ2);
            vertices.push_back(ZVX1); vertices.push_back(ZVY1); vertices.push_back(ZVZ2);

            vertices.push_back(ZVX1); vertices.push_back(ZVY2); vertices.push_back(ZVZ1);
            vertices.push_back(ZVX1); vertices.push_back(ZVY2); vertices.push_back(ZVZ2);
            vertices.push_back(ZVX2); vertices.push_back(ZVY2); vertices.push_back(ZVZ2);
            vertices.push_back(ZVX2); vertices.push_back(ZVY2); vertices.push_back(ZVZ1);

            indices.push_back(firstIndex + 0);
            indices.push_back(firstIndex + 1);
            indices.push_back(firstIndex + 2);
            indices.push_back(firstIndex + 3);

            indices.push_back(firstIndex + 4);
            indices.push_back(firstIndex + 5);
            indices.push_back(firstIndex + 6);
            indices.push_back(firstIndex + 7);

            indices.push_back(firstIndex + 3);
            indices.push_back(firstIndex + 2);
            indices.push_back(firstIndex + 6);
            indices.push_back(firstIndex + 5);

            indices.push_back(firstIndex + 0);
            indices.push_back(firstIndex + 3);
            indices.push_back(firstIndex + 5);
            indices.push_back(firstIndex + 4);

            indices.push_back(firstIndex + 0);
            indices.push_back(firstIndex + 4);
            indices.push_back(firstIndex + 7);
            indices.push_back(firstIndex + 1);

            indices.push_back(firstIndex + 1);
            indices.push_back(firstIndex + 7);
            indices.push_back(firstIndex + 6);
            indices.push_back(firstIndex + 2);
        }
    }

    FILE* fhandle = fopen("output.obj", "w+");

    for (int i = 0; i < vertices.size(); i += 3)
    {
        fprintf(fhandle, "v %f %f %f\n", -vertices[i + 0], vertices[i + 2], vertices[i + 1]);
    }

    for (int i = 0; i < indices.size(); i += 4)
    {
        fprintf(fhandle, "f %d %d %d %d\n", indices[i + 0], indices[i + 1], indices[i + 2], indices[i + 3]);
    }

    fclose(fhandle);

}

#ifdef USE_IMGUI
void InputS16(const char* name, s16* value)
{
    int intValue = *value;

    ImGui::InputInt(name, &intValue);

    *value = intValue;
}
#endif

void debugger_draw(void)
{
#ifdef USE_IMGUI
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("Debug"))
        {
            ImGui::MenuItem("No Collisions", nullptr, &debuggerVar_noHardClip);
            ImGui::Combo("Collision", (int*)&hardColDisplayMode, "None\0Wireframe\0Filled\0");
            ImGui::Combo("Triggers", (int*)&sceColDisplayMode, "None\0Wireframe\0Filled\0");
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    if(debuggerVar_debugMenuDisplayed)
    {
        if(cameraDataTable.size())
        {
            cameraDataStruct* pCamera = cameraDataTable[NumCamera];

            ImGui::Begin("Camera");
            if (pCamera)
            {
                ImGui::BeginGroup();
                ImGui::PushID("Position");
                ImGui::InputInt("X", &translateX);
                ImGui::InputInt("Y", &translateY);
                ImGui::InputInt("Z", &translateZ);
                ImGui::PopID();
                ImGui::EndGroup();

                ImGui::BeginGroup();
                ImGui::PushID("Center");
                InputS16("Pitch", &pCamera->alpha);
                InputS16("Yaw", &pCamera->beta);
                InputS16("Roll", &pCamera->gamma);
                ImGui::PopID();
                ImGui::EndGroup();

                SetAngleCamera(pCamera->alpha, pCamera->beta, pCamera->gamma);

                ImGui::BeginGroup();
                ImGui::PushID("Center");
                ImGui::InputInt("HCenter", &cameraCenterX);
                ImGui::InputInt("VCenter", &cameraCenterY);
                ImGui::PopID();
                ImGui::EndGroup();

                ImGui::BeginGroup();
                ImGui::PushID("Projection");
                ImGui::InputInt("Perspective", &cameraPerspective);
                ImGui::InputInt("XFov", &cameraFovX);
                ImGui::InputInt("YFov", &cameraFovY);
                ImGui::PopID();
                ImGui::EndGroup();
            }
            if (ImGui::Button("DumpScene"))
            {
                SaveScene();
            }
            ImGui::End();
        }


        if(ListWorldObjets.size())
        {
            ImGui::Begin("World objects");

            static int selectedWorldObject = 0;
            ImGui::InputInt("Index", &selectedWorldObject);

            ImGui::Separator();

            if (selectedWorldObject > ListWorldObjets.size())
                selectedWorldObject = ListWorldObjets.size() - 1;

            tWorldObject* pWorldObject = &ListWorldObjets[selectedWorldObject];

            if(pWorldObject)
            {
                InputS16("objectIndex", &pWorldObject->objIndex);
                InputS16("body", &pWorldObject->body);
                InputS16("flags", &pWorldObject->flags);
                InputS16("typeZV", &pWorldObject->typeZV);
                InputS16("foundBody", &pWorldObject->foundBody);
                InputS16("foundName", &pWorldObject->foundName);
                InputS16("flags2", &pWorldObject->foundFlag);
                InputS16("foundLife", &pWorldObject->foundLife);
                InputS16("x", &pWorldObject->x);
                InputS16("y", &pWorldObject->y);
                InputS16("z", &pWorldObject->z);
                InputS16("alpha", &pWorldObject->alpha);
                InputS16("beta", &pWorldObject->beta);
                InputS16("gamma", &pWorldObject->gamma);
                InputS16("stage", &pWorldObject->stage);
                InputS16("room", &pWorldObject->room);
                InputS16("lifeMode", &pWorldObject->lifeMode);
                InputS16("life", &pWorldObject->life);
                InputS16("floorLife", &pWorldObject->floorLife);
                InputS16("anim", &pWorldObject->anim);
                InputS16("frame", &pWorldObject->frame);
                InputS16("animType", &pWorldObject->animType);
                InputS16("animInfo", &pWorldObject->animInfo);
                InputS16("trackMode", &pWorldObject->trackMode);
                InputS16("trackNumber", &pWorldObject->trackNumber);
                InputS16("positionInTrack", &pWorldObject->positionInTrack);
                InputS16("mark", &pWorldObject->mark);
            }

            ImGui::End();
        }

        {
            ImGui::Begin("Active objects");

            static int selectedObject = 0;
            ImGui::InputInt("Index", &selectedObject);

            //ImGui::Separator();

            if (selectedObject > NUM_MAX_OBJECT)
                selectedObject = NUM_MAX_OBJECT - 1;

            tObject*pObject = &ListObjets[selectedObject];

            ImGui::PushItemWidth(100);
            
            InputS16("world index", &pObject->indexInWorld); ImGui::SameLine();
            InputS16("bodyNum", &pObject->bodyNum);
//            InputS16("_flags", &pObject->_flags);
            InputS16("dynFlags", &pObject->dynFlags);
            //ZVStruct zv;
            InputS16("screenXMin", &pObject->screenXMin); ImGui::SameLine();
            InputS16("screenYMin", &pObject->screenYMin); ImGui::SameLine();
            InputS16("screenXMax", &pObject->screenXMax); ImGui::SameLine();
            InputS16("screenYMax", &pObject->screenYMax);
            
            InputS16("roomX", &pObject->roomX); ImGui::SameLine();
            InputS16("roomY", &pObject->roomY); ImGui::SameLine();
            InputS16("roomZ", &pObject->roomZ); ImGui::SameLine();
            
            InputS16("worldX", &pObject->worldX);
            InputS16("worldY", &pObject->worldY);
            InputS16("worldZ", &pObject->worldZ);
            
            InputS16("alpha", &pObject->alpha);
            InputS16("beta", &pObject->beta);
            InputS16("gamma", &pObject->gamma);
            
            InputS16("stage", &pObject->stage);
            InputS16("room", &pObject->room);
            
            InputS16("lifeMode", &pObject->lifeMode);
            InputS16("life", &pObject->life);
            //unsigned int CHRONO;
            //unsigned int ROOM_CHRONO;
            InputS16("ANIM", &pObject->ANIM);
            InputS16("animType", &pObject->animType);
            InputS16("animInfo", &pObject->animInfo);
            InputS16("newAnim", &pObject->newAnim);
            InputS16("newAnimType", &pObject->newAnimType);
            InputS16("newAnimInfo", &pObject->newAnimInfo);
            InputS16("FRAME", &pObject->frame);
            InputS16("numOfFrames", &pObject->numOfFrames);
            InputS16("END_FRAME", &pObject->END_FRAME);
            InputS16("END_ANIM", &pObject->flagEndAnim);
            InputS16("trackMode", &pObject->trackMode);
            InputS16("trackNumber", &pObject->trackNumber);
            InputS16("MARK", &pObject->MARK);
            InputS16("positionInTrack", &pObject->positionInTrack);

            InputS16("stepX", &pObject->stepX);
            InputS16("stepY", &pObject->stepY);
            InputS16("stepZ", &pObject->stepZ);

            InputS16("animNegX", &pObject->animNegX);
            InputS16("animNegY", &pObject->animNegY);
            InputS16("animNegZ", &pObject->animNegZ);

            //interpolatedValue YHandler;
            InputS16("falling", &pObject->falling);
            //interpolatedValue rotate;
            InputS16("direction", &pObject->direction);
            InputS16("speed", &pObject->speed);
            //interpolatedValue speedChange;
            //s16 COL[3];
            InputS16("COL_BY", &pObject->COL_BY);
            InputS16("HARD_DEC", &pObject->HARD_DEC);
            InputS16("HARD_COL", &pObject->HARD_COL);
            InputS16("HIT", &pObject->HIT);
            InputS16("HIT_BY", &pObject->HIT_BY);
            InputS16("animActionType", &pObject->animActionType);
            InputS16("animActionANIM", &pObject->animActionANIM);
            InputS16("animActionFRAME", &pObject->animActionFRAME);
            InputS16("animActionParam", &pObject->animActionParam);
            InputS16("hitForce", &pObject->hitForce);
            InputS16("hotPointID", &pObject->hotPointID);
            //point3dStruct hotPoint;
            InputS16("hardMat", &pObject->hardMat);

            ImGui::PopItemWidth();

            ImGui::End();
        }

        // Post-Processing Controls (only shown when HD backgrounds are enabled)
        if (g_postProcessing && g_remasterConfig.graphics.enableHDBackgrounds)
        {
            ImGui::Begin("Post-Processing");

            ImGui::Text("Effects (HD Backgrounds Mode)");
            ImGui::Separator();

            // Bloom controls
            bool bloomEnabled = g_postProcessing->isBloomEnabled();
            if (ImGui::Checkbox("Enable Bloom", &bloomEnabled))
            {
                g_postProcessing->setBloomEnabled(bloomEnabled);
            }

            if (bloomEnabled)
            {
                ImGui::Indent();
                static float bloomThreshold = 0.8f;
                static float bloomIntensity = 1.0f;

                if (ImGui::SliderFloat("Bloom Threshold", &bloomThreshold, 0.0f, 1.0f))
                {
                    g_postProcessing->setBloomThreshold(bloomThreshold);
                }
                ImGui::SetItemTooltip("Higher = only brightest lights bloom");

                if (ImGui::SliderFloat("Bloom Intensity", &bloomIntensity, 0.0f, 2.0f))
                {
                    g_postProcessing->setBloomIntensity(bloomIntensity);
                }
                ImGui::SetItemTooltip("How strong the glow effect is");

                ImGui::Unindent();
            }

            ImGui::Spacing();

            // Film grain controls
            bool grainEnabled = g_postProcessing->isFilmGrainEnabled();
            if (ImGui::Checkbox("Enable Film Grain", &grainEnabled))
            {
                g_postProcessing->setFilmGrainEnabled(grainEnabled);
            }

            if (grainEnabled)
            {
                ImGui::Indent();
                static float grainIntensity = 0.05f;

                if (ImGui::SliderFloat("Grain Intensity", &grainIntensity, 0.0f, 0.2f))
                {
                    g_postProcessing->setFilmGrainIntensity(grainIntensity);
                }
                ImGui::SetItemTooltip("Retro VHS/film aesthetic");

                ImGui::Unindent();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Text("Recommended Presets:");

            if (ImGui::Button("Subtle Horror"))
            {
                g_postProcessing->setBloomEnabled(true);
                g_postProcessing->setBloomThreshold(0.85f);
                g_postProcessing->setBloomIntensity(0.9f);
                g_postProcessing->setFilmGrainEnabled(true);
                g_postProcessing->setFilmGrainIntensity(0.06f);
            }
            ImGui::SameLine();
            if (ImGui::Button("Dramatic"))
            {
                g_postProcessing->setBloomEnabled(true);
                g_postProcessing->setBloomThreshold(0.7f);
                g_postProcessing->setBloomIntensity(1.3f);
                g_postProcessing->setFilmGrainEnabled(true);
                g_postProcessing->setFilmGrainIntensity(0.08f);
            }
            ImGui::SameLine();
            if (ImGui::Button("Clean"))
            {
                g_postProcessing->setBloomEnabled(false);
                g_postProcessing->setFilmGrainEnabled(false);
            }

            ImGui::End();
        }
    }
#endif
}

#endif
