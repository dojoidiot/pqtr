import React from 'react';
import { NavigationContainer } from '@react-navigation/native';
import HomeTabNavigator from './HomeTabNavigator';

const AppNavigator = () => {
  return (
    <NavigationContainer>
      <HomeTabNavigator />
    </NavigationContainer>
  );
};

export default AppNavigator; 