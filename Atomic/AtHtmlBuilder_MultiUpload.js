const idPrefix = arg.idPrefix;
const section = Elem(idPrefix + 'Section');

let nrDivs = 0;
let lastDivNr = 0;

const MultiUpload_AddDiv = function () {
    const nr = lastDivNr + 1;
    const inElemId = idPrefix + nr;

    const inDiv = document.createElement('div');
    const inElem = document.createElement('input');

    inElem.setAttribute('type', 'file');
    inElem.setAttribute('id', inElemId);
    inElem.setAttribute('name', inElemId);
    inElem.setAttribute('multiple', '');
    inElem.addEventListener('change', function () {
        MultiUpload_OnAction(inDiv, inElem, nr);
    });

    inDiv.appendChild(inElem);
    section.appendChild(inDiv);

    lastDivNr = nr;
    nrDivs = nrDivs + 1;
};

const MultiUpload_OnAction = function (inDiv, inElem, nr) {
    if (inElem.value != '' && nr == lastDivNr) {
        let rmLink = document.createElement('a');
        rmLink.setAttribute('href', '#');
        rmLink.innerHTML = 'Remove';
        rmLink.addEventListener('click', function (e) {
            MultiUpload_RemoveDiv(inDiv);
            e.preventDefault();
        });

        inDiv.appendChild(rmLink);
        MultiUpload_AddDiv();
    }
};

const MultiUpload_RemoveDiv = function (inDiv) {
    inDiv.parentElement.removeChild(inDiv);
    nrDivs = nrDivs - 1;
    if (nrDivs == 0) {
        MultiUpload_AddDiv();
    }
};

MultiUpload_AddDiv();
