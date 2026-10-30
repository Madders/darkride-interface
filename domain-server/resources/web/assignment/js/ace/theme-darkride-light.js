/* ***** BEGIN LICENSE BLOCK *****
 * Distributed under the BSD license:
 *
 * Copyright (c) 2010, Ajax.org B.V.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Ajax.org B.V. nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL AJAX.ORG B.V. BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 *  theme-darkride-light.js
 *  domain-server/resources/web/assignment/js/ace
 *
 *  Created on 2026-10-19.
 *
 *  The script editor's light theme, for the world server's light look: Darkride's light page colours, with code in
 *  its cyan and violet. Made from Ace's twilight theme beside it (theme-twilight.js), whose notice is above.
 *
 *  Copyright 2026 Darkride.
 *
 *  Distributed under the MIT License.
 *  See the accompanying file LICENSE-MIT or https://opensource.org/licenses/MIT
 */

ace.define('ace/theme/darkride-light', ['require', 'exports', 'module' , 'ace/lib/dom'], function(require, exports, module) {

exports.isDark = false;
exports.cssClass = "ace-darkride-light";
exports.cssText = ".ace-darkride-light .ace_gutter {\
background: #eef1f7;\
color: #8a91ab\
}\
.ace-darkride-light .ace_print-margin {\
width: 1px;\
background: #e3e8f4\
}\
.ace-darkride-light {\
background-color: #ffffff;\
color: #1c2136\
}\
.ace-darkride-light .ace_cursor {\
border-left: 2px solid #0080c0\
}\
.ace-darkride-light .ace_overwrite-cursors .ace_cursor {\
border-left: 0px;\
border-bottom: 1px solid #0080c0\
}\
.ace-darkride-light .ace_marker-layer .ace_selection {\
background: rgba(0, 175, 255, 0.18)\
}\
.ace-darkride-light.ace_multiselect .ace_selection.ace_start {\
box-shadow: 0 0 3px 0px #ffffff;\
border-radius: 2px\
}\
.ace-darkride-light .ace_marker-layer .ace_step {\
background: rgb(255, 236, 160)\
}\
.ace-darkride-light .ace_marker-layer .ace_bracket {\
margin: -1px 0 0 -1px;\
border: 1px solid rgba(28, 33, 54, 0.3)\
}\
.ace-darkride-light .ace_marker-layer .ace_active-line {\
background: rgba(0, 175, 255, 0.06)\
}\
.ace-darkride-light .ace_gutter-active-line {\
background-color: rgba(0, 175, 255, 0.08)\
}\
.ace-darkride-light .ace_marker-layer .ace_selected-word {\
border: 1px solid rgba(0, 175, 255, 0.35)\
}\
.ace-darkride-light .ace_invisible {\
color: rgba(28, 33, 54, 0.25)\
}\
.ace-darkride-light .ace_keyword,\
.ace-darkride-light .ace_meta {\
color: #7b2fd1\
}\
.ace-darkride-light .ace_constant,\
.ace-darkride-light .ace_constant.ace_character,\
.ace-darkride-light .ace_constant.ace_character.ace_escape,\
.ace-darkride-light .ace_constant.ace_other,\
.ace-darkride-light .ace_heading,\
.ace-darkride-light .ace_support.ace_constant {\
color: #c2410c\
}\
.ace-darkride-light .ace_invalid.ace_illegal {\
color: #ffffff;\
background-color: rgba(217, 54, 54, 0.85)\
}\
.ace-darkride-light .ace_invalid.ace_deprecated {\
text-decoration: underline;\
font-style: italic;\
color: #d93636\
}\
.ace-darkride-light .ace_support {\
color: #5b6382\
}\
.ace-darkride-light .ace_fold {\
background-color: #0080c0;\
border-color: #1c2136\
}\
.ace-darkride-light .ace_support.ace_function {\
color: #0080c0\
}\
.ace-darkride-light .ace_list,\
.ace-darkride-light .ace_storage {\
color: #7b2fd1\
}\
.ace-darkride-light .ace_entity.ace_name.ace_function,\
.ace-darkride-light .ace_meta.ace_tag {\
color: #0080c0\
}\
.ace-darkride-light .ace_string {\
color: #15803d\
}\
.ace-darkride-light .ace_string.ace_regexp {\
color: #b45309\
}\
.ace-darkride-light .ace_comment {\
font-style: italic;\
color: #8a91ab\
}\
.ace-darkride-light .ace_variable {\
color: #1d4ed8\
}\
.ace-darkride-light .ace_xml-pe {\
color: #8a91ab\
}\
.ace-darkride-light .ace_indent-guide {\
box-shadow: inset -1px 0 0 #e6e9f2\
}";

var dom = require("../lib/dom");
dom.importCssString(exports.cssText, exports.cssClass);
});
