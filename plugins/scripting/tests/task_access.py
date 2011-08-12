#!/usr/bin/env kross
# -*- coding: utf-8 -*-

import traceback
import Kross
import Plan
import TestResult


TestResult.setResult( True )
asserttext1 = "Test of property '{0}' failed:\n   Expected: '{2}'\n        Got: '{1}'"
asserttext2 = "Failed to set property '{0}' to '{1}'"

try:
    project = Plan.project()
    assert project is not None, "Project not found"
    
    task = project.createTask( 0 )
    assert task is not None, "Could not create task"
    project.addCommand( "Create task" );
    
    data = "Task name"
    property = 'Name'
    before = project.data(task, property)
    res = project.setData(task, property, data)
    text = asserttext2.format(property, data)
    assert res == True, text
    result = project.data(task, property)
    text = asserttext1.format(property, result, data)
    assert result == data, text
    project.revertCommand()
    result = project.data(task, property)
    text = asserttext1.format(property, result, before)
    assert result == before, text
    
    data = "Task responsible"
    property = 'Responsible'
    before = project.data(task, property)
    res = project.setData(task, property, data)
    text = asserttext2.format(property, data)
    assert res == True, text
    result = project.data(task, property)
    text = asserttext1.format(property, result, data)
    assert result == data, text
    project.revertCommand()
    result = project.data(task, property)
    text = asserttext1.format(property, result, before)
    assert result == before, text

    property = 'Allocation'
    data = "John Doe"
    before = project.data(task, property)
    res = project.setData(task, property, data)
    text = asserttext2.format(property, data)
    assert res == True, text
    result = project.data(task, property, 'DisplayRole', -1)
    text = asserttext1.format(property, result, data)
    assert result == data, text
    project.revertCommand()
    result = project.data(task, property)
    text = asserttext1.format(property, result, before)
    assert result == before, text

    property = 'Estimate'
    data = "3.0h"
    before = project.data(task, property)
    res = project.setData(task, property, data)
    text = asserttext2.format(property, data)
    assert res == True, text
    result = project.data(task, property, 'DisplayRole', -1)
    text = asserttext1.format(property, result, data)
    assert result == data, text
    project.revertCommand()
    result = project.data(task, property)
    text = asserttext1.format(property, result, before)
    assert result == before, text

    property = 'EstimateType'
    data = "Duration"
    before = project.data(task, property)
    res = project.setData(task, property, data)
    text = asserttext2.format(property, data)
    assert res == True, text
    result = project.data(task, property, 'DisplayRole', -1)
    text = asserttext1.format(property, result, data)
    assert result == data, text
    project.revertCommand()
    result = project.data(task, property)
    text = asserttext1.format(property, result, before)
    assert result == before, text

    property = 'Constraint'
    data = "ALAP"
    before = project.data(task, property)
    res = project.setData(task, property, data)
    text = asserttext2.format(property, data)
    assert res == True, text
    result = project.data(task, property, 'EditRole', -1)
    text = asserttext1.format(property, result, data)
    assert result == data, text
    project.revertCommand()
    result = project.data(task, property)
    text = asserttext1.format(property, result, before)
    assert result == before, text

    property = 'Constraint'
    data = "FixedInterval"
    res = project.setData(task, property, data)
    text = asserttext2.format(property, data)
    assert res == True, text
    result = project.data(task, property, 'EditRole', -1)
    text = asserttext1.format(property, result, data)
    assert result == data, text
    project.addCommand("Set permanent")
    
    property = 'ConstraintStart'
    data = "2011-08-01T10:00:00"
    before = project.data(task, property)
    res = project.setData(task, property, data)
    text = asserttext2.format(property, data)
    assert res == True, text
    result = project.data(task, property, 'EditRole', -1)
    text = asserttext1.format(property, result, data)
    assert result == data, text
    project.revertCommand()
    result = project.data(task, property)
    text = asserttext1.format(property, result, before)
    assert result == before, text

    property = 'ConstraintEnd'
    data = "2011-08-01T11:00:00"
    before = project.data(task, property)
    res = project.setData(task, property, data)
    text = asserttext2.format(property, data)
    assert res == True, text
    result = project.data(task, property, 'EditRole', -1)
    text = asserttext1.format(property, result, data)
    assert result == data, text
    project.revertCommand()
    result = project.data(task, property)
    text = asserttext1.format(property, result, before)
    assert result == before, text

    property = 'OptimisticRatio'
    data = -20
    before = project.data(task, property)
    res = project.setData(task, property, data)
    text = asserttext2.format(property, data)
    assert res == True, text
    result = project.data(task, property, 'EditRole', -1)
    text = asserttext1.format(property, result, data)
    assert result == data, text
    project.revertCommand()
    result = project.data(task, property)
    text = asserttext1.format(property, result, before)
    assert result == before, text

    property = 'PessimisticRatio'
    data = 120
    before = project.data(task, property)
    res = project.setData(task, property, data)
    text = asserttext2.format(property, data)
    assert res == True, text
    result = project.data(task, property, 'EditRole', -1)
    text = asserttext1.format(property, result, data)
    assert result == data, text
    project.revertCommand()
    result = project.data(task, property)
    text = asserttext1.format(property, result, before)
    assert result == before, text

    property = 'Constraint'
    data = "ASAP"
    res = project.setData(task, property, data)
    text = asserttext2.format(property, data)
    assert res == True, text
    result = project.data(task, property, 'EditRole', -1)
    text = asserttext1.format(property, result, data)
    assert result == data, text
    project.addCommand("Set permanent")

    property = 'Estimate'
    data = "3.0d"
    res = project.setData(task, property, data)
    text = asserttext2.format(property, data)
    assert res == True, text
    result = project.data(task, property, 'DisplayRole', -1)
    text = asserttext1.format(property, result, data)
    assert result == data, text
    project.addCommand( "" )

    property = 'Risk'
    data = 'Low'
    before = project.data(task, property)
    res = project.setData(task, property, data)
    text = asserttext2.format(property, data)
    assert res == True, text
    result = project.data(task, property, 'EditRole', -1)
    text = asserttext1.format(property, result, data)
    assert result == data, text
    project.revertCommand()
    result = project.data(task, property)
    text = asserttext1.format(property, result, before)
    assert result == before, text

    property = 'StartupCost'
    data = '$ 1,000.00'
    before = project.data(task, property)
    res = project.setData(task, property, data)
    text = asserttext2.format(property, data)
    assert res == True, text
    result = project.data(task, property, 'DisplayRole', -1)
    text = asserttext1.format(property, result, data)
    assert result == data, text
    project.revertCommand()
    result = project.data(task, property)
    text = asserttext1.format(property, result, before)
    assert result == before, text

    property = 'ShutdownCost'
    data = 1000.00
    before = project.data(task, property, 'EditRole' , -1)
    res = project.setData(task, property, data)
    text = asserttext2.format(property, data)
    assert res == True, text
    result = project.data(task, property, 'EditRole', -1)
    text = asserttext1.format(property, result, data)
    assert result == data, text
    project.revertCommand()
    result = project.data(task, property, 'EditRole', -1)
    text = asserttext1.format(property, result, before)
    assert result == before, text

    property = 'Description'
    data = 'Task description'
    before = project.data(task, property)
    res = project.setData(task, property, data)
    text = asserttext2.format(property, data)
    assert res == True, text
    result = project.data(task, property, 'EditRole', -1)
    text = asserttext1.format(property, result, data)
    assert result == data, text
    project.revertCommand()
    result = project.data(task, property)
    text = asserttext1.format(property, result, before)
    assert result == before, text

except:
    TestResult.setResult( False )
    TestResult.setMessage("\n" + traceback.format_exc(1))
