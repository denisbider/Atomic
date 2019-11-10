const checkboxElem = Elem(arg.checkboxId);
const otherElem = Elem(arg.otherId);

checkboxElem.addEventListener('click', function () {
    otherElem.disabled = !checkboxElem.checked;
});
