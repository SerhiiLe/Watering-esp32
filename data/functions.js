function ajaxRequest(url, aMethod, param, onSuccess, onFailure) {
	var aR = new XMLHttpRequest();
	aR.onreadystatechange = function() {
		if( aR.readyState == 4 && (aR.status == 200 || aR.status == 304))
			onSuccess(aR);
		else if (aR.readyState == 4 && aR.status != 200) onFailure(aR);
	};
	aR.open(aMethod, url, true);
	if (aMethod == 'POST') aR.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded; charset=utf-8');
	aR.send(param);
};
function $g(aID) {return (aID !== '' ? document.getElementById(aID) : null);}
function $gn(aID) {return (aID != '' ? document.getElementsByName(aID) : null);}
function $gt(str,m) {return (typeof m == 'undefined' ? document:m).getElementsByTagName(str);}
function $gc(str,m) {return (typeof m == 'undefined' ? document:m).getElementsByClassName(str);}
function $at(aElem, att) {if (att !== undefined) {for (var xi in att) {aElem.setAttribute(xi, att[xi]); if (xi.toUpperCase() == 'TITLE') aElem.setAttribute('alt', att[xi]);}}}
function $t(iHTML) {return document.createTextNode(iHTML);}
function $e(nElem, att) {var Elem = document.createElement(nElem); $at(Elem, att); return Elem;}
function $ee(nElem, oElem, att) {var Elem = $e(nElem, att); if (oElem !== undefined) if( typeof(oElem) == 'object' ) Elem.appendChild(oElem); else Elem.innerHTML = oElem; return Elem;}
function $am(Elem, mElem) {if (mElem !== undefined) for(var i = 0; i < mElem.length; i++) { if( typeof(mElem[i]) == 'object' ) Elem.appendChild(mElem[i]); else Elem.appendChild($t(mElem[i])); } return Elem;}
function $em(nElem, mElem, att) {var Elem = $e(nElem, att); return $am(Elem, mElem);}
function insertAfter(node, rN) {rN.parentNode.insertBefore(node, rN.nextSibling);}
function $ib(node, rN) {rN.parentNode.insertBefore(node, rN);}
function dummy() {return;}
jsVoid = 'javaScript:void(0)';
jsNone = 'return false;';

function httpGet(url) {
	var xhttp = new XMLHttpRequest();
	xhttp.open("GET", url, false);
	xhttp.send(null);
	return xhttp.responseText;
};

function toggle_by_class(cls, on) {
	var lst = $gc(cls);
	for(var i = 0; i < lst.length; ++i) {
		lst[i].style.display = on ? '' : 'none';
	}
};
function ret() {
	$g("return").removeAttribute("onclick");
	$g("list").style.display = "table";
	$g("edit").style.display = "none";
};
function toggle_edit(n,url) {
	var e = $g("e"+n);
	if( e.checked ) {
		show_edit(n);
		e.checked = false;
	} else {
		$g("t"+n).classList.add("off");
		ajaxRequest(url, "POST", "t="+n, dummy, dummy);
	}
};
function onoff(id,a=1) {
	ajaxRequest("onoff","POST","t="+id+"&a="+a, function(ajaxResp) {
		$g(id).innerHTML = ajaxResp.responseText=="1" ? "On": "Off";
	}, dummy);
};
function fill_settings(url,form_name,cbfunc=null) {
	ajaxRequest(url,"GET",null, function(ajaxResp) {
		var doc = JSON.parse(ajaxResp.responseText);
		var f = document.forms[form_name];
		for (var key in doc) {
			if(!f.elements[key]) continue;
			if(f.elements[key].type=="checkbox") {
				if( f.elements[key].checked && doc[key] == 0 )
					f.elements[key].checked = false;
				if( ! f.elements[key].checked && doc[key] != 0 )
					f.elements[key].checked = true;
			} else if(f.elements[key].type=="time") {
				var h = Math.floor(doc[key]/60);
				var m = doc[key]%60;
				f.elements[key].value = (h<10?"0"+h:h) + ":" + (m<10?"0"+m:m);
			} else
				f.elements[key].value = doc[key];
		}
		if(cbfunc !== null) cbfunc();
	}, dummy);
};
function hide_missing_hardware() {
	ajaxRequest("hw_info", "GET", null, function(ajaxResp) {
		var doc = JSON.parse(ajaxResp.responseText);
		for (var key in doc) toggle_by_class(key, doc[key]);
	}, dummy);
}