{
    const inputIds = [ 'smtpSender_relayHost', 'smtpSender_relayPort', 'smtpSender_relayImplicitTls', 'smtpSender_relayTlsRequirement',
        'smtpSender_relayAuthType', 'smtpSender_relayUsername', 'smtpSender_relayPassword' ];

    const checkbox = Elem('smtpSender_useRelay');

    const OnCheckbox = function () {
        inputIds.forEach(function (id) {
            Elem(id).disabled = !checkbox.checked;
        });
    };

    checkbox.addEventListener('click', OnCheckbox);
    OnCheckbox();
}
