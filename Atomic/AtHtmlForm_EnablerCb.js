const enablerCbElem = Elem(arg.enablerCbId);

enablerCbElem.addEventListener('click', function () {
    for (let i=0; i<arg.enablerForFieldIds.length; ++i) {
        let fieldId = arg.enablerForFieldIds[i];
        let container = Elem(fieldId + "_container");

             if (!enablerCbElem.checked)                  container.style.display = 'none';
        else if (container.tagName.toLowerCase() == "tr") container.style.display = 'table-row';
        else                                              container.style.display = 'block';
    }
});
