import React from 'react';
import { createNativeStackNavigator } from '@react-navigation/native-stack';
import ProjectNameScreen from '../screens/create-project/ProjectNameScreen';
import SelectCameraScreen from '../screens/create-project/SelectCameraScreen';
import ApplyPresetScreen from '../screens/create-project/ApplyPresetScreen';
import ExportSettingsScreen from '../screens/create-project/ExportSettingsScreen';
import FinalShootScreen from '../screens/create-project/FinalShootScreen';

export type CreateProjectStackParamList = {
  ProjectName: undefined;
  SelectCamera: { projectName: string };
  ApplyPreset: { projectName: string; camera: string };
  ExportSettings: { projectName: string; camera: string; preset: string };
  FinalShoot: { projectName: string; camera: string; preset: string; exportFormats: string[]; autoExport: boolean };
};

const Stack = createNativeStackNavigator<CreateProjectStackParamList>();

export default function CreateProjectNavigator() {
  return (
    <Stack.Navigator
      screenOptions={{
        headerShown: false,
        animation: 'slide_from_right',
      }}
    >
      <Stack.Screen name="ProjectName" component={ProjectNameScreen} />
      <Stack.Screen name="SelectCamera" component={SelectCameraScreen} />
      <Stack.Screen name="ApplyPreset" component={ApplyPresetScreen} />
      <Stack.Screen name="ExportSettings" component={ExportSettingsScreen} />
      <Stack.Screen name="FinalShoot" component={FinalShootScreen} />
    </Stack.Navigator>
  );
}
