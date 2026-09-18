// META: script=/resources/WebIDLParser.js
// META: script=/resources/idlharness.js

'use strict';

idl_test(['../wpt_internal/persistent-widgets/resources/persistent-widgets'],
         ['html', 'dom'], idl_array => {
           idl_array.add_objects({
             HTMLPersistentWidgetElement:
                 ['document.createElement("persistentwidget")'],
             Window: ['window'],
             HTMLBodyElement: ['document.body'],
           });
         });
