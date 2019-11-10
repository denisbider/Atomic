function ReloadReqHandler(req, outElemId, statusElemId, method, url, retryMs) {
	if (req.readyState == 4) {
		if (req.status == 200 || req.status == 203 || req.status == 503) {
			if (req.status != 503) {
				Elem(outElemId).innerHTML = r.responseText;
				Elem(statusElemId).innerHTML = '';
			}
			if (req.status != 200) {
				setTimeout(function() { return SendReloadReq(outElemId, statusElemId, method, url, retryMs); }, retryMs);
			}
		} else {
			Elem(statusElemId).innerHTML = '[ Dynamic page update failed: Unexpected HTTP response code: ' + req.status + ']';
		}
	}
}

function SendReloadReq(outElemId, statusElemId, method, url, retryMs) {
	var req = new XMLHttpRequest;
	if (req == null) {
		Elem(statusElemId).innerHTML = '[ Dynamic page update failed: XMLHttpRequest is null ]';
	} else {
		req.open(method, url, true);
		req.onreadystatechange = function() { return ReloadReqHandler(req, outElemId, statusElemId, method, url, retryMs); };
		req.send();
	}
}

function ReloadElem(outElemId, statusElemId, method, url, firstWaitMs, retryMs) {
	setTimeout(function() { return SendReloadReq(outElemId, statusElemId, method, url, retryMs); }, firstWaitMs);
}
